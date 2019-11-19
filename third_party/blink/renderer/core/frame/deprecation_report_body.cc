// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/deprecation_report_body.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

void DeprecationReportBody::BuildJSONValue(V8ObjectBuilder& builder) const {
  LocationReportBody::BuildJSONValue(builder);
  builder.AddString("id", id());
  builder.AddString("message", message());

  bool is_null = false;
  double anticipated_removal_value = anticipatedRemoval(is_null);
  if (is_null) {
    builder.AddNull("anticipatedRemoval");
  } else {
    DateComponents anticipated_removal_date;
    bool is_valid =
        anticipated_removal_date.SetMillisecondsSinceEpochForDateTimeLocal(
            anticipated_removal_value);
    if (!is_valid) {
      builder.AddNull("anticipatedRemoval");
    } else {
      // Adding extra 'Z' here to ensure that the string gives the same result
      // as JSON.stringify(anticipatedRemoval) in javascript. Note here
      // anticipatedRemoval will become a Date object in javascript.
      String iso8601_date =
          anticipated_removal_date.ToString(DateComponents::kMillisecond) + "Z";
      builder.AddString("anticipatedRemoval", iso8601_date);
    }
  }
}

}  // namespace blink
