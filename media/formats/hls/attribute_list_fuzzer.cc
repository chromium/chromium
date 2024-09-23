// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "base/check.h"
#include "media/formats/hls/types.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  using media::hls::ParseStatusCode;
  using media::hls::SourceString;
  using media::hls::types::AttributeListIterator;
  using media::hls::types::AttributeMap;

  std::string_view str(reinterpret_cast<const char*>(data), size);

  // Try parsing attribute list for a tag with lots of attributes
  auto attributes = AttributeMap::MakeStorage(
      "ALLOWED-CPC", "AUDIO", "AVERAGE-BANDWIDTH", "BANDWIDTH",
      "CLOSED-CAPTIONS", "CODECS", "FRAME-RATE", "HDCP-LEVEL",
      "PATHWAY-ID"
      "RESOLUTION",
      "SCORE", "STABLE-VARIANT-ID", "SUBTITLES", "VIDEO-RANGE");
  AttributeMap map(attributes);
  AttributeListIterator iter(SourceString::CreateForTesting(str));

  auto error = map.FillUntilError(&iter);
  CHECK(error == ParseStatusCode::kReachedEOF ||
        error == ParseStatusCode::kMalformedAttributeList ||
        error == ParseStatusCode::kAttributeListHasDuplicateNames);
  return 0;
}
