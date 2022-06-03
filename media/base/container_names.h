// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CONTAINER_NAMES_H_
#define MEDIA_BASE_CONTAINER_NAMES_H_

#include <stdint.h>

#include "media/base/media_export.h"

namespace media {

namespace container_names {

// This is the set of input container formats detected for logging purposes. Not
// all of these are enabled (and it varies by product). Any additions need to be
// done at the end of the list (before CONTAINER_MAX). This list must be kept in
// sync with the enum definition "MediaContainers" in
// tools/metrics/histograms/histograms.xml.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
// GENERATED_JAVA_PREFIX_TO_STRIP: CONTAINER_
enum MediaContainerName {
  CONTAINER_UNKNOWN,       // Unknown
  CONTAINER_AAC,           // AAC (Advanced Audio Coding)
  CONTAINER_AC3,           // AC-3
  CONTAINER_AIFF,          // AIFF (Audio Interchange File Format)
  CONTAINER_AMR,           // AMR (Adaptive Multi-Rate Audio)
  CONTAINER_APE,           // APE (Monkey's Audio)
  CONTAINER_ASF,           // ASF (Advanced / Active Streaming Format)
  CONTAINER_ASS,           // SSA (SubStation Alpha) subtitle
  CONTAINER_AVI,           // AVI (Audio Video Interleaved)
  CONTAINER_BINK,          // Bink
  CONTAINER_CAF,           // CAF (Apple Core Audio Format)
  CONTAINER_DTS,           // DTS
  CONTAINER_DTSHD,         // DTS-HD
  CONTAINER_DV,            // DV (Digital Video)
  CONTAINER_DXA,           // DXA
  CONTAINER_EAC3,          // Enhanced AC-3
  CONTAINER_FLAC,          // FLAC (Free Lossless Audio Codec)
  CONTAINER_FLV,           // FLV (Flash Video)
  CONTAINER_GSM,           // GSM (Global System for Mobile Audio)
  CONTAINER_H261,          // H.261
  CONTAINER_H263,          // H.263
  CONTAINER_H264,          // H.264
  CONTAINER_HLS,           // HLS (Apple HTTP Live Streaming PlayList)
  CONTAINER_IRCAM,         // Berkeley/IRCAM/CARL Sound Format
  CONTAINER_MJPEG,         // MJPEG video
  CONTAINER_MOV,           // QuickTime / MOV / MPEG4
  CONTAINER_MP3,           // MP3 (MPEG audio layer 2/3)
  CONTAINER_MPEG2PS,       // MPEG-2 Program Stream
  CONTAINER_MPEG2TS,       // MPEG-2 Transport Stream
  CONTAINER_MPEG4BS,       // MPEG-4 Bitstream
  CONTAINER_OGG,           // Ogg
  CONTAINER_RM,            // RM (RealMedia)
  CONTAINER_SRT,           // SRT (SubRip subtitle)
  CONTAINER_SWF,           // SWF (ShockWave Flash)
  CONTAINER_VC1,           // VC-1
  CONTAINER_WAV,           // WAV / WAVE (Waveform Audio)
  CONTAINER_WEBM,          // Matroska / WebM
  CONTAINER_WTV,           // WTV (Windows Television)
  CONTAINER_DASH,          // DASH (MPEG-DASH)
  CONTAINER_SMOOTHSTREAM,  // SmoothStreaming
  CONTAINER_MAX = CONTAINER_SMOOTHSTREAM,  // Must be last
};

// Minimum size considered for processing.
enum { kMinimumContainerSize = 12 };

// Determine the container type.
MEDIA_EXPORT MediaContainerName DetermineContainer(const uint8_t* buffer,
                                                   int buffer_size);

}  // namespace container_names

}  // namespace media

#endif  // MEDIA_BASE_CONTAINER_NAMES_H_
