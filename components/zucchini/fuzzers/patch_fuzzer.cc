// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <optional>

#include "components/zucchini/buffer_view.h"
#include "components/zucchini/patch_reader.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  logging::SetMinLogLevel(3);  // Disable console spamming.
  zucchini::ConstBufferView patch(data, size);
  std::optional<zucchini::EnsemblePatchReader> patch_reader =
      zucchini::EnsemblePatchReader::Create(patch);
  return 0;
}
