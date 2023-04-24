// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_debug_utils.h"

#include <memory>
#include <numeric>
#include <string>
#include <utility>

namespace blink {

namespace {

std::string NewlineToSpaceReplacer(std::string str) {
  std::replace(str.begin(), str.end(), '\n', ' ');
  return str;
}

}  // namespace

std::string TreeToStringHelper(const AXObject* obj, int indent, bool verbose) {
  if (!obj) {
    return "";
  }

  return std::accumulate(
      obj->CachedChildrenIncludingIgnored().begin(),
      obj->CachedChildrenIncludingIgnored().end(),
      std::string(2 * indent, ' ') +
          NewlineToSpaceReplacer(obj->ToString(verbose).Utf8()) + "\n",
      [indent, verbose](const std::string& str, const AXObject* child) {
        return str + TreeToStringHelper(child, indent + 1, verbose);
      });
}

}  // namespace blink
