// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PARSED_PARAMS_H_
#define PDF_PARSED_PARAMS_H_

#include <string>

#include "pdf/pdfium/pdfium_form_filler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
struct WebPluginParams;
}

namespace chrome_pdf {

struct ParsedParams {
  ParsedParams();
  ParsedParams(const ParsedParams& other);
  ~ParsedParams();

  // The document original URL. Must not be empty.
  std::string original_url;

  // The plugin source URL. Must not be empty.
  std::string src_url;

  // The background color for the PDF viewer.
  absl::optional<SkColor> background_color;

  // Whether the plugin should occupy the entire frame.
  bool full_frame = false;

  // Whether to execute JavaScript and maybe XFA.
  PDFiumFormFiller::ScriptOption script_option =
      PDFiumFormFiller::DefaultScriptOption();
};

// Creates an `ParsedParams` by parsing a `blink::WebPluginParams`. If
// `blink::WebPluginParams` is invalid, returns absl::nullopt.
absl::optional<ParsedParams> ParseWebPluginParams(
    const blink::WebPluginParams& params);

}  // namespace chrome_pdf

#endif  // PDF_PARSED_PARAMS_H_
