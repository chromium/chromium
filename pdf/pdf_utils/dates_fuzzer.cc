// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_utils/dates.h"

#include <stdint.h>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  chrome_pdf::ParsePdfDate(base::as_string_view(data));
  return 0;
}
