// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_utils.h"

#include <string>

#include "base/debug/dump_without_crashing.h"
#include "base/json/json_writer.h"
#include "base/values.h"

namespace content {

std::string SerializeAttributionJson(base::ValueView body, bool pretty_print) {
  int options = pretty_print ? base::JSONWriter::OPTIONS_PRETTY_PRINT : 0;

  std::string output_json;
  if (!base::JSONWriter::WriteWithOptions(body, options, &output_json)) {
    base::debug::DumpWithoutCrashing();
  }
  return output_json;
}

}  // namespace content
