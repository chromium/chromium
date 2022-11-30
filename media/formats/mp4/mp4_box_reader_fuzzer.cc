// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/check.h"
#include "media/base/media_util.h"
#include "media/formats/mp4/box_reader.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  media::NullMediaLog media_log;
  std::unique_ptr<media::mp4::BoxReader> reader;
  if (media::mp4::BoxReader::ReadTopLevelBox(data, size, &media_log, &reader) ==
      media::mp4::ParseResult::kOk) {
    CHECK(reader);
    std::ignore = reader->ScanChildren();
  }
  return 0;
}
