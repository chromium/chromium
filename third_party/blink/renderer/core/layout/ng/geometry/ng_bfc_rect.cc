// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_rect.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

String NGBfcRect::ToString() const {
  StringBuilder buidler;
  buidler.Append(start_offset.ToString());
  buidler.Append('+');
  buidler.Append(end_offset.ToString());
  return buidler.ToString();
}

std::ostream& operator<<(std::ostream& os, const NGBfcRect& value) {
  return os << value.ToString();
}

}  // namespace blink
