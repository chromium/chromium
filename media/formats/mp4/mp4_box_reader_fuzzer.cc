// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/check.h"
#include "base/containers/span.h"
#include "media/base/media_util.h"
#include "media/formats/mp4/box_reader.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

// Entry point for LibFuzzer.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  media::NullMediaLog media_log;
  std::unique_ptr<media::mp4::BoxReader> reader;
  if (media::mp4::BoxReader::ReadTopLevelBox(data, &media_log, &reader) ==
      media::mp4::ParseResult::kOk) {
    CHECK(reader);
    std::ignore = reader->ScanChildren();
  }
  return 0;
}
