// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/server_timing_header.h"

#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

void ServerTimingHeader::SetParameter(StringView name, String value) {
  if (EqualIgnoringAsciiCase(name, "dur")) {
    if (!duration_set_) {
      duration_ = StringToDouble(value).value_or(0);
      duration_set_ = true;
    }
  } else if (EqualIgnoringAsciiCase(name, "desc")) {
    if (!description_set_) {
      description_ = value;
      description_set_ = true;
    }
  }
}

}  // namespace blink
