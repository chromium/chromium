// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/text_affinity.h"

#include <ostream>

#include "third_party/blink/public/web/web_ax_enums.h"

namespace blink {

std::ostream& operator<<(std::ostream& ostream, TextAffinity affinity) {
  switch (affinity) {
    case TextAffinity::kDownstream:
      return ostream << "TextAffinity::Downstream";
    case TextAffinity::kUpstream:
      return ostream << "TextAffinity::Upstream";
  }
  return ostream << "TextAffinity(" << static_cast<int>(affinity) << ')';
}

}  // namespace blink
