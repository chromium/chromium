// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/parsed_params.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "pdf/pdfium/pdfium_form_filler.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_plugin_params.h"

namespace chrome_pdf {

ParsedParams::ParsedParams() = default;

ParsedParams::ParsedParams(const ParsedParams& other) = default;
ParsedParams& ParsedParams::operator=(const ParsedParams& other) = default;

ParsedParams::ParsedParams(ParsedParams&& other) noexcept = default;
ParsedParams& ParsedParams::operator=(ParsedParams&& other) noexcept = default;

ParsedParams::~ParsedParams() = default;

std::optional<ParsedParams> ParseWebPluginParams(
    const blink::WebPluginParams& params) {
  ParsedParams result;
  for (size_t i = 0; i < params.attribute_names.size(); ++i) {
    if (params.attribute_names[i] == "src") {
      result.src_url = params.attribute_values[i].Utf8();
    } else if (params.attribute_names[i] == "original-url") {
      result.original_url = params.attribute_values[i].Utf8();
    } else if (params.attribute_names[i] == "top-level-url") {
      result.top_level_url = params.attribute_values[i].Utf8();
    } else if (params.attribute_names[i] == "full-frame") {
      result.full_frame = true;
    } else if (params.attribute_names[i] == "background-color") {
      if (!base::StringToUint(params.attribute_values[i].Utf8(),
                              &result.background_color)) {
        return std::nullopt;
      }
    } else if (params.attribute_names[i] == "javascript") {
      if (params.attribute_values[i] != "allow")
        result.script_option = PDFiumFormFiller::ScriptOption::kNoJavaScript;
    } else if (params.attribute_names[i] == "has-edits") {
      result.has_edits = true;
    } else if (params.attribute_names[i] == "use-skia") {
      result.use_skia = true;
    }
  }

  if (result.src_url.empty())
    return std::nullopt;

  if (result.original_url.empty())
    result.original_url = result.src_url;

  return result;
}

}  // namespace chrome_pdf
