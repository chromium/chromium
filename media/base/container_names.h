// Copyright 2013 The Chromium Authors
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
enum class MediaContainerName {
  kContainerUnknown,       // Unknown
  kContainerAAC,           // AAC (Advanced Audio Coding)
  kContainerAC3,           // AC-3
  kContainerAIFF,          // AIFF (Audio Interchange File Format)
  kContainerAMR,           // AMR (Adaptive Multi-Rate Audio)
  kContainerAPE,           // APE (Monkey's Audio)
  kContainerASF,           // ASF (Advanced / Active Streaming Format)
  kContainerASS,           // SSA (SubStation Alpha) subtitle
  kContainerAVI,           // AVI (Audio Video Interleaved)
  kContainerBink,          // Bink
  kContainerCAF,           // CAF (Apple Core Audio Format)
  kContainerDTS,           // DTS
  kContainerDTSHD,         // DTS-HD
  kContainerDV,            // DV (Digital Video)
  kContainerDXA,           // DXA
  kContainerEAC3,          // Enhanced AC-3
  kContainerFLAC,          // FLAC (Free Lossless Audio Codec)
  kContainerFLV,           // FLV (Flash Video)
  kContainerGSM,           // GSM (Global System for Mobile Audio)
  kContainerH261,          // H.261
  kContainerH263,          // H.263
  kContainerH264,          // H.264
  kContainerHLS,           // HLS (Apple HTTP Live Streaming PlayList)
  kContainerIRCAM,         // Berkeley/IRCAM/CARL Sound Format
  kContainerMJPEG,         // MJPEG video
  kContainerMOV,           // QuickTime / MOV / MPEG4
  kContainerMP3,           // MP3 (MPEG audio layer 2/3)
  kContainerMPEG2PS,       // MPEG-2 Program Stream
  kContainerMPEG2TS,       // MPEG-2 Transport Stream
  kContainerMPEG4BS,       // MPEG-4 Bitstream
  kContainerOgg,           // Ogg
  kContainerRM,            // RM (RealMedia)
  kContainerSRT,           // SRT (SubRip subtitle)
  kContainerSWF,           // SWF (ShockWave Flash)
  kContainerVC1,           // VC-1
  kContainerWAV,           // WAV / WAVE (Waveform Audio)
  kContainerWEBM,          // Matroska / WebM
  kContainerWTV,           // WTV (Windows Television)
  kContainerDASH,          // DASH (MPEG-DASH)
  kContainerSmoothStream,  // SmoothStreaming
  kMaxValue = kContainerSmoothStream,  // Must be last
};

// Minimum size considered for processing.
enum { kMinimumContainerSize = 12 };

// Determine the container type.
MEDIA_EXPORT MediaContainerName DetermineContainer(const uint8_t* buffer,
                                                   int buffer_size);

}  // namespace container_names

}  // namespace media

#endif  // MEDIA_BASE_CONTAINER_NAMES_H_
