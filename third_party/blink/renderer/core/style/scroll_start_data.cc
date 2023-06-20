// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/scroll_start_data.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

#if DCHECK_IS_ON()
String ScrollStartData::ToString() const {
  return WTF::String::Format("{type: %i, length:%s}",
                             static_cast<int>(value_type),
                             value.ToString().Ascii().c_str());
}
#endif

}  // namespace blink
