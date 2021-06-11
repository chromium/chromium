// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/parsed_params.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_plugin_params.h"

namespace chrome_pdf {

ParsedParams::ParsedParams() = default;

ParsedParams::ParsedParams(const ParsedParams& other) = default;

ParsedParams::~ParsedParams() = default;

absl::optional<ParsedParams> ParseWebPluginParams(
    const blink::WebPluginParams& params) {
  ParsedParams result;
  for (size_t i = 0; i < params.attribute_names.size(); ++i) {
    if (params.attribute_names[i] == "src") {
      result.original_url = params.attribute_values[i].Utf8();
    } else if (params.attribute_names[i] == "stream-url") {
      result.stream_url = params.attribute_values[i].Utf8();
    } else if (params.attribute_names[i] == "full-frame") {
      result.full_frame = true;
    } else if (params.attribute_names[i] == "background-color") {
      SkColor background_color;
      if (!base::StringToUint(params.attribute_values[i].Utf8(),
                              &background_color)) {
        return absl::nullopt;
      }
      result.background_color = background_color;
    }
  }

  if (result.original_url.empty())
    return absl::nullopt;

  if (result.stream_url.empty())
    result.stream_url = result.original_url;

  return result;
}

}  // namespace chrome_pdf
