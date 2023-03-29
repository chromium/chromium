// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/internals_ukm_recorder.h"
#include <cstddef>
#include <vector>
#include "base/functional/bind.h"
#include "components/ukm/test_ukm_recorder.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

InternalsUkmRecorder::InternalsUkmRecorder(Document* document)
    : source_id_(document->UkmSourceID()) {}

HeapVector<ScriptValue> InternalsUkmRecorder::getMetrics(
    ScriptState* script_state,
    const String& entry_name,
    const Vector<String>& metric_names) {
  std::vector<std::string> names(metric_names.size());
  std::transform(metric_names.begin(), metric_names.end(), names.begin(),
                 [](String name) { return std::string(name.Utf8()); });

  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> entries =
      recorder_.GetEntries(entry_name.Utf8(), names);
  HeapVector<ScriptValue> result;
  for (const ukm::TestUkmRecorder::HumanReadableUkmEntry& entry : entries) {
    if (entry.source_id != source_id_) {
      continue;
    }

    V8ObjectBuilder builder(script_state);
    for (const auto& iterator : entry.metrics) {
      builder.AddNumber(String(iterator.first), iterator.second);
    }
    result.push_back(builder.GetScriptValue());
  }

  return result;
}

}  // namespace blink
