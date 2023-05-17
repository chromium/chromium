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

std::string TreeToStringWithHelper(const AXObject* obj,
                                   int indent,
                                   bool verbose) {
  return TreeToStringWithMarkedObjectHelper(obj, nullptr, indent, verbose);
}

std::string TreeToStringWithMarkedObjectHelper(const AXObject* obj,
                                               const AXObject* marked_object,
                                               int indent,
                                               bool verbose) {
  if (!obj) {
    return "";
  }

  std::string extra = obj == marked_object ? "*" : " ";
  return std::accumulate(
      obj->CachedChildrenIncludingIgnored().begin(),
      obj->CachedChildrenIncludingIgnored().end(),
      extra + std::string(std::max(2 * indent - 1, 0), ' ') +
          NewlineToSpaceReplacer(obj->ToString(verbose).Utf8()) + "\n",
      [indent, verbose, marked_object](const std::string& str,
                                       const AXObject* child) {
        return str + TreeToStringWithMarkedObjectHelper(child, marked_object,
                                                        indent + 1, verbose);
      });
}

}  // namespace blink
