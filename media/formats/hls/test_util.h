// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_TEST_UTIL_H_
#define MEDIA_FORMATS_HLS_TEST_UTIL_H_

#include "base/strings/string_piece.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/types.h"

namespace media::hls {

inline types::VariableName CreateVarName(base::StringPiece name) {
  return types::VariableName::Parse(SourceString::CreateForTesting(name))
      .value();
}

inline types::ByteRange CreateByteRange(types::DecimalInteger length,
                                        types::DecimalInteger offset) {
  return types::ByteRange::Validate(length, offset).value();
}

}  // namespace media::hls

#endif
