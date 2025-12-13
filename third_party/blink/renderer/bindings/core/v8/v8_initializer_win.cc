// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"

#include "base/json/json_reader.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/re2/src/re2/re2.h"
#include "v8/include/v8.h"

namespace blink {

v8::FilterETWSessionByURLResult FilterETWSessionByURLCallback(
    v8::Local<v8::Context> context,
    const std::string& json_payload) {
  std::optional<base::Value> optional_value = base::JSONReader::Read(
      json_payload, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!optional_value || !optional_value.value().is_dict()) {
    return {false, false};  // Invalid payload
  }

  const base::Value::Dict& dict = optional_value.value().GetDict();

  std::optional<bool> opt_trace_interpreter_frames =
      dict.FindBool("trace_interpreter_frames");
  bool trace_interpreter_frames = opt_trace_interpreter_frames.has_value() &&
                                  opt_trace_interpreter_frames.value();

  const base::Value::List* filtered_urls = dict.FindList("filtered_urls");
  if (!filtered_urls) {
    return {false, false};  // Invalid payload
  }

  ExecutionContext* execution_context = ToExecutionContext(context);
  if (!execution_context) {
    return {false, false};  // No URL
  }
  std::string url(execution_context->Url().GetString().Utf8());

  for (size_t i = 0; i < filtered_urls->size(); i++) {
    const base::Value& filtered_url = (*filtered_urls)[i];
    if (!filtered_url.is_string()) {
      return {false, false};  // Invalid payload
    }

    const RE2 regex(filtered_url.GetString());
    if (RE2::FullMatch(url, regex)) {
      return {true, trace_interpreter_frames};
    }
  }
  return {false, false};  // No regex matching found.
}

}  // namespace blink
