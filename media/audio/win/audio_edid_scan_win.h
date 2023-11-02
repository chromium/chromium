// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_AUDIO_EDID_SCAN_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_EDID_SCAN_WIN_H_

#include <stdint.h>

#include "media/base/media_export.h"

namespace media {

// The WMI service allows the querying of monitor-type devices which report
// Extended Display Identification Data (EDID).  The WMI service can be
// queried for a list of COM objects which represent the "paths" which
// are associated with individual EDID devices.  Querying each of those
// paths using the WmiGetMonitorRawEEdidV1Block method returns the EDID
// blocks for those devices.  We query the extended blocks which contain
// the Short Audio Descriptor (SAD), and parse them to obtain a bitmask
// indicating which audio content is supported.  The mask consists of
// AudioParameters::Format flags.  If multiple EDID devices are present,
// the intersection of flags is reported.
MEDIA_EXPORT uint32_t ScanEdidBitstreams();

// Bitmask returned by ScanEdidBitstreams.  Set bits indicate detected
// audio passthrough support.
enum : uint32_t {
  kAudioBitstreamPcmLinear = 0x001,  // PCM is 'raw' amplitude samples.
  kAudioBitstreamDts = 0x002,        // Compressed DTS bitstream.
  kAudioBitstreamDtsHd = 0x004,      // Compressed DTS-HD bitstream.
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_EDID_SCAN_WIN_H_
