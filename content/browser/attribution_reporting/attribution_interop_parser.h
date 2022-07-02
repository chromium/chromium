// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTEROP_PARSER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTEROP_PARSER_H_

#include <stddef.h>

#include <iosfwd>
#include <string>

#include "base/callback_forward.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_parser_test_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Parser for the interop test.
// See //content/test/data/attribution_reporting/simulator/README.md and
// //content/test/data/attribution_reporting/interop/README.md for the input
// and output JSON schema.
class AttributionInteropParser {
 public:
  explicit AttributionInteropParser(std::ostream& stream);
  ~AttributionInteropParser();

  AttributionInteropParser(const AttributionInteropParser&) = delete;
  AttributionInteropParser(AttributionInteropParser&&) = delete;

  AttributionInteropParser& operator=(const AttributionInteropParser&) = delete;
  AttributionInteropParser& operator=(AttributionInteropParser&&) = delete;

  // Converts interop test input to simulator input format.
  absl::optional<base::Value> SimulatorInputFromInteropInput(
      base::Value::Dict& input);

  // Converts simulator output to interop test output format.
  absl::optional<base::Value> InteropOutputFromSimulatorOutput(
      base::Value output);

 private:
  bool has_error() const;

  [[nodiscard]] AttributionParserErrorManager::ScopedContext PushContext(
      AttributionParserErrorManager::Context context);

  AttributionParserErrorManager::ErrorWriter Error();

  void MoveDictValues(base::Value::Dict& in, base::Value::Dict& out);

  void MoveValue(base::Value::Dict& in,
                 base::StringPiece in_key,
                 base::Value::Dict& out,
                 absl::optional<base::StringPiece> out_key_opt = absl::nullopt);

  bool EnsureDictionary(const base::Value* value);

  absl::optional<std::string> ExtractString(base::Value::Dict& dict,
                                            base::StringPiece key);

  void ParseList(base::Value* values,
                 base::RepeatingCallback<void(base::Value)> callback,
                 size_t expected_size = 0);

  // Returns `attribution_src_url` in the request if exists.
  absl::optional<std::string> ParseRequest(base::Value::Dict& in,
                                           base::Value::Dict& out);

  void ParseResponse(base::Value::Dict& in,
                     base::Value::Dict& out,
                     const std::string& attribution_src_url);

  base::Value::List ParseEvents(base::Value::Dict& dict, base::StringPiece key);

  base::Value::List ParseEventLevelReports(base::Value::Dict& output);

  base::Value::List ParseAggregatableReports(base::Value::Dict& output);

  AttributionParserErrorManager error_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTEROP_PARSER_H_
