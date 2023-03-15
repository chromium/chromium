// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_WIN_MEDIA_FOUNDATION_UTILS_H_
#define MEDIA_FILTERS_WIN_MEDIA_FOUNDATION_UTILS_H_

#include <mfidl.h>

#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"
#include "media/base/subsample_entry.h"
#include "media/media_buildflags.h"

class IMFMediaType;

namespace media {

// Given an AudioDecoderConfig, get its corresponding IMFMediaType format.
// Note:
// IMFMediaType is derived from IMFAttributes and hence all the of information
// in a media type is store as attributes.
// https://docs.microsoft.com/en-us/windows/win32/medfound/media-type-attributes
// has a list of media type attributes.
MEDIA_EXPORT HRESULT
GetDefaultAudioType(const AudioDecoderConfig decoder_config,
                    IMFMediaType** media_type_out);

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// Given an AudioDecoderConfig which represents AAC audio, get its
// corresponding IMFMediaType format (by calling GetDefaultAudioType)
// and populate the aac_extra_data in the decoder_config into the
// returned IMFMediaType.
MEDIA_EXPORT HRESULT GetAacAudioType(const AudioDecoderConfig decoder_config,
                                     IMFMediaType** media_type_out);
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

// A wrapper of SubsampleEntry for MediaFoundation. The data blob associated
// with MFSampleExtension_Encryption_SubSample_Mapping attribute should contain
// an array of byte ranges as DWORDs where every two DWORDs make a set.
// SubsampleEntry has a set of uint32_t that needs to be converted to DWORDs.
struct MediaFoundationSubsampleEntry {
  explicit MediaFoundationSubsampleEntry(SubsampleEntry entry)
      : clear_bytes(entry.clear_bytes), cipher_bytes(entry.cypher_bytes) {}
  MediaFoundationSubsampleEntry() = default;
  DWORD clear_bytes = 0;
  DWORD cipher_bytes = 0;
};

// Converts between MFTIME and TimeDelta. MFTIME defines units of 100
// nanoseconds. See
// https://learn.microsoft.com/en-us/windows/win32/medfound/mftime
MEDIA_EXPORT MFTIME TimeDeltaToMfTime(base::TimeDelta time);
MEDIA_EXPORT base::TimeDelta MfTimeToTimeDelta(MFTIME mf_time);

}  // namespace media

#endif  // MEDIA_FILTERS_WIN_MEDIA_FOUNDATION_UTILS_H_
