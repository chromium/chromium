// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SAMPLE_FORMAT_H_
#define MEDIA_BASE_SAMPLE_FORMAT_H_

#include "media/base/media_export.h"

namespace media {

// Sample formats.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SampleFormat)
enum SampleFormat {
  // These values are histogrammed over time; do not change their ordinal
  // values.  When deleting a sample format replace it with a dummy value; when
  // adding a sample format, do so at the bottom before kSampleFormatMax, and
  // update the value of kSampleFormatMax.
  kUnknownSampleFormat = 0,
  kSampleFormatU8 = 1,           // Unsigned 8-bit w/ bias of 128.
  kSampleFormatS16 = 2,          // Signed 16-bit.
  kSampleFormatS32 = 3,          // Signed 32-bit.
  kSampleFormatF32 = 4,          // Float 32-bit.
  kSampleFormatPlanarS16 = 5,    // Signed 16-bit planar.
  kSampleFormatPlanarF32 = 6,    // Float 32-bit planar.
  kSampleFormatPlanarS32 = 7,    // Signed 32-bit planar.
  kSampleFormatS24 = 8,          // Signed 24-bit.
  kSampleFormatAc3 = 9,          // Compressed AC3 bitstream.
  kSampleFormatEac3 = 10,        // Compressed E-AC3 bitstream.
  kSampleFormatMpegHAudio = 11,  // Compressed MPEG-H audio bitstream.
  kSampleFormatPlanarU8 = 12,    // Unsigned 8-bit w/ bias of 128 planar.
  kSampleFormatDts = 13,         // Compressed DTS audio bitstream.
  kSampleFormatDtsxP2 = 14,      // Compressed DTSX audio bitstream.
  kSampleFormatIECDts = 15,      // IEC-61937 encapsulated DTS audio bitstream.
  kSampleFormatDtse = 16,        // Compressed DTS Express audio bitstream.

  // Must always be equal to largest value ever logged.
  kMaxValue = kSampleFormatDtse,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:SampleFormat)

// Returns the number of bytes used per channel for the specified
// |sample_format|.
MEDIA_EXPORT int SampleFormatToBytesPerChannel(SampleFormat sample_format);
MEDIA_EXPORT int SampleFormatToBitsPerChannel(SampleFormat sample_format);

// Returns the name of the sample format as a string
MEDIA_EXPORT const char* SampleFormatToString(SampleFormat sample_format);

// Returns true if |sample_format| is planar, false otherwise.
MEDIA_EXPORT bool IsPlanar(SampleFormat sample_format);

// Returns true if |sample_format| is interleaved, false otherwise.
MEDIA_EXPORT bool IsInterleaved(SampleFormat sample_format);

// Returns true if |sample_format| is compressed bitstream, false otherwise.
MEDIA_EXPORT bool IsBitstream(SampleFormat sample_format);

}  // namespace media

#endif  // MEDIA_BASE_SAMPLE_FORMAT_H_
