// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/subtree_paint_property_update_reason.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

unsigned operator&(unsigned mask, SubtreePaintPropertyUpdateReason reason) {
  return mask & static_cast<unsigned>(reason);
}

}  // namespace

String SubtreePaintPropertyUpdateReasonsToString(unsigned bitmask) {
  StringBuilder result;
  bool need_separator = false;
  auto append = [&result, &need_separator](const char* name) {
    if (need_separator)
      result.Append("|");
    result.Append(name);
    need_separator = true;
  };

  result.Append("(");
  if (bitmask == static_cast<unsigned>(SubtreePaintPropertyUpdateReason::kNone))
    append("kNone");
  if (bitmask & SubtreePaintPropertyUpdateReason::kContainerChainMayChange)
    append("kContainerChainMayChange");
  if (bitmask & SubtreePaintPropertyUpdateReason::kPreviouslySkipped)
    append("kPreviouslySkipped");
  if (bitmask & SubtreePaintPropertyUpdateReason::kPrinting)
    append("kPrinting");
  if (bitmask & SubtreePaintPropertyUpdateReason::kTransformStyleChanged)
    append("kTransformStyleChanged");
  result.Append(")");
  return result.ToString();
}

}  // namespace blink
