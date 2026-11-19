// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_AV1_SVC_UTILS_H_
#define MEDIA_PARSERS_AV1_SVC_UTILS_H_

#include "media/base/media_export.h"
#include "third_party/libgav1/src/src/obu_parser.h"
#include "third_party/libgav1/src/src/utils/vector.h"

// Helpers shared by the AV1 decoders and bitstream inspectors that need to
// recognize spatial scalability (SVC) and the AV1 RTP payload format's
// wildcard operating point. They all look at operating point 0.

namespace media {

// Returns true if `sequence_header` has a single operating point with
// operating_point_idc[0] = 0xFFF, the AV1 RTP payload format wildcard
// operating point (AV1 RTP Specification Section 5). The level is not checked:
// the specification pairs the wildcard with seq_level_idx[0] = 0, and
// ParseOneAV1Frame() checks for that itself.
MEDIA_EXPORT bool IsAV1RtpWildcardOperatingPoint(
    const libgav1::ObuSequenceHeader& sequence_header);

// Returns true iff `sequence_header` explicitly specifies multiple spatial
// layers for operating point 0. Temporal-only scalability (e.g. L1T3) and the
// RTP wildcard operating point return false.
MEDIA_EXPORT bool IsAV1SpatialLayerSequence(
    const libgav1::ObuSequenceHeader& sequence_header);

// Returns true if any of `obu_headers` belongs to a spatial layer above the
// base layer (spatial_id > 0).
MEDIA_EXPORT bool HasAV1SpatialLayerObu(
    const libgav1::Vector<libgav1::ObuHeader>& obu_headers);

// Calls `parser.ParseOneFrame()`, but ignores the level conformance failure
// that ObuParser reports for frames above Level 2.0 when the sequence uses the
// RTP wildcard operating point with seq_level_idx[0] = 0, since that level
// does not describe the stream.
MEDIA_EXPORT libgav1::StatusCode ParseOneAV1Frame(
    libgav1::ObuParser& parser,
    libgav1::RefCountedBufferPtr* current_frame);

}  // namespace media

#endif  // MEDIA_PARSERS_AV1_SVC_UTILS_H_
