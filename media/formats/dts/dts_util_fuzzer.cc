// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/dts/dts_util.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/containers/span.h"
#include "media/base/media_util.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view data_view(reinterpret_cast<const char*>(data), size);
  media::dts::ParseTotalSampleCount(base::as_byte_span(data_view),
                                    media::AudioCodec::kDTS);
  return 0;
}
