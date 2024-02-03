// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/deprecation/deprecation_report_body.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

ScriptValue DeprecationReportBody::anticipatedRemoval(
    ScriptState* script_state) const {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (!anticipated_removal_)
    return ScriptValue::CreateNull(isolate);
  return ScriptValue(isolate, ToV8Traits<IDLNullable<IDLDate>>::ToV8(
                                  script_state, *anticipated_removal_));
}

std::optional<base::Time> DeprecationReportBody::AnticipatedRemoval() const {
  return anticipated_removal_;
}

void DeprecationReportBody::BuildJSONValue(V8ObjectBuilder& builder) const {
  LocationReportBody::BuildJSONValue(builder);
  builder.AddString("id", id());
  builder.AddString("message", message());

  if (!anticipated_removal_) {
    builder.AddNull("anticipatedRemoval");
  } else {
    DateComponents anticipated_removal_date;
    bool is_valid =
        anticipated_removal_date.SetMillisecondsSinceEpochForDateTimeLocal(
            anticipated_removal_->InMillisecondsFSinceUnixEpochIgnoringNull());
    if (!is_valid) {
      builder.AddNull("anticipatedRemoval");
    } else {
      // Adding extra 'Z' here to ensure that the string gives the same result
      // as JSON.stringify(anticipatedRemoval) in javascript. Note here
      // anticipatedRemoval will become a Date object in javascript.
      String iso8601_date = anticipated_removal_date.ToString(
                                DateComponents::SecondFormat::kMillisecond) +
                            "Z";
      builder.AddString("anticipatedRemoval", iso8601_date);
    }
  }
}

}  // namespace blink
