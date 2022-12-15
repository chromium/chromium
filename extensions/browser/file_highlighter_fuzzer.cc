// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/browser/file_highlighter.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  const std::string contents = provider.ConsumeRandomLengthString();

  std::unique_ptr<extensions::FileHighlighter> highlighter;
  if (provider.ConsumeBool()) {
    const std::string key = provider.ConsumeRandomLengthString();
    const std::string specific = provider.ConsumeRandomLengthString();
    highlighter = std::make_unique<extensions::ManifestHighlighter>(
        contents, key, specific);
  } else {
    const size_t line_number = provider.ConsumeIntegral<size_t>();
    highlighter =
        std::make_unique<extensions::SourceHighlighter>(contents, line_number);
  }

  CHECK_EQ(highlighter->GetBeforeFeature() + highlighter->GetFeature() +
               highlighter->GetAfterFeature(),
           contents);

  return 0;
}
