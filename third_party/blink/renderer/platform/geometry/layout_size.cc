// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/layout_size.h"

#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

std::ostream& operator<<(std::ostream& ostream, const LayoutSize& size) {
  return ostream << size.ToString();
}

String LayoutSize::ToString() const {
  return String::Format("%sx%s", Width().ToString().Ascii().c_str(),
                        Height().ToString().Ascii().c_str());
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const LayoutSize& size) {
  return ts << gfx::SizeF(size).ToString();
}

}  // namespace blink
