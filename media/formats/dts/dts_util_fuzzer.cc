// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/check.h"
#include "media/base/media_util.h"
#include "media/formats/dts/dts_util.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  media::dts::ParseTotalSampleCount(data, size, media::AudioCodec::kDTS);
  return 0;
}
