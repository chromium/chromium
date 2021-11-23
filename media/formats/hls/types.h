// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_TYPES_H_
#define MEDIA_FORMATS_HLS_TYPES_H_

#include <cstdint>
#include "base/strings/string_piece.h"
#include "media/formats/hls/parse_status.h"

namespace media {
namespace hls {
namespace types {

// Data-types used in HLS, as described by the spec
using DecimalInteger = std::uint64_t;
using DecimalFloatingPoint = double;
using SignedDecimalFloatingPoint = double;

ParseStatus::Or<DecimalInteger> MEDIA_EXPORT
ParseDecimalInteger(base::StringPiece str);

ParseStatus::Or<DecimalFloatingPoint> MEDIA_EXPORT
ParseDecimalFloatingPoint(base::StringPiece str);

ParseStatus::Or<SignedDecimalFloatingPoint> MEDIA_EXPORT
ParseSignedDecimalFloatingPoint(base::StringPiece str);

}  // namespace types
}  // namespace hls
}  // namespace media

#endif  // MEDIA_FORMATS_HLS_TYPES_H_
