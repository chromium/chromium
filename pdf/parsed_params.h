// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PARSED_PARAMS_H_
#define PDF_PARSED_PARAMS_H_

#include <optional>
#include <string>

#include "pdf/pdfium/pdfium_form_filler.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
struct WebPluginParams;
}  // namespace blink

namespace chrome_pdf {

struct ParsedParams {
  ParsedParams();
  ParsedParams(const ParsedParams& other);
  ParsedParams& operator=(const ParsedParams& other);
  ParsedParams(ParsedParams&& other) noexcept;
  ParsedParams& operator=(ParsedParams&& other) noexcept;
  ~ParsedParams();

  // The plugin source URL. Must not be empty.
  std::string src_url;

  // The document original URL. Must not be empty.
  std::string original_url;

  // The top-level URL.
  std::string top_level_url;

  // Whether the plugin should occupy the entire frame.
  bool full_frame = false;

  // The background color for the PDF viewer.
  SkColor background_color = SK_ColorTRANSPARENT;

  // Whether to execute JavaScript and maybe XFA.
  PDFiumFormFiller::ScriptOption script_option =
      PDFiumFormFiller::DefaultScriptOption();

  // Whether the PDF was edited previously in annotation mode.
  bool has_edits = false;

  // Whether the PDF viewer uses Skia renderer. When set to false, the PDF
  // viewer uses AGG renderer.
  bool use_skia = false;
};

// Creates an `ParsedParams` by parsing a `blink::WebPluginParams`. If
// `blink::WebPluginParams` is invalid, returns std::nullopt.
std::optional<ParsedParams> ParseWebPluginParams(
    const blink::WebPluginParams& params);

}  // namespace chrome_pdf

#endif  // PDF_PARSED_PARAMS_H_
