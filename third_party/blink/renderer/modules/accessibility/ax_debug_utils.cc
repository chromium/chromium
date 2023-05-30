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
  return TreeToStringWithMarkedObjectHelper(obj, nullptr, verbose);
}

std::string TreeToStringWithMarkedObjectHelperRecursive(
    const AXObject* obj,
    const AXObject* marked_object,
    bool cached,
    int indent,
    bool verbose,
    int* marked_object_found_count) {
  if (!obj) {
    return "";
  }

  if (marked_object_found_count && marked_object && obj == marked_object) {
    ++*marked_object_found_count;
  }
  std::string extra = obj == marked_object ? "*" : " ";
  return std::accumulate(
      obj->CachedChildrenIncludingIgnored().begin(),
      obj->CachedChildrenIncludingIgnored().end(),
      extra + std::string(std::max(2 * indent - 1, 0), ' ') +
          NewlineToSpaceReplacer(obj->ToString(verbose, cached).Utf8()) + "\n",
      [cached, indent, verbose, marked_object, marked_object_found_count](
          const std::string& str, const AXObject* child) {
        return str + TreeToStringWithMarkedObjectHelperRecursive(
                         child, marked_object, cached, indent + 1, verbose,
                         marked_object_found_count);
      });
}

std::string TreeToStringWithMarkedObjectHelper(const AXObject* obj,
                                               const AXObject* marked_object,
                                               bool verbose) {
  int marked_object_found_count = 0;
  // Use cached properties only unless it's frozen and thus safe to use compute
  // methods.
  bool cached = !obj->IsDetached() && !obj->AXObjectCache().IsFrozen();

  std::string tree_str = TreeToStringWithMarkedObjectHelperRecursive(
      obj, marked_object, cached, 0, verbose, &marked_object_found_count);
  if (marked_object_found_count == 1) {
    return tree_str;
  }

  return std::string("**** ERROR: Found marked objects was found ") +
         String::Number(marked_object_found_count).Utf8() +
         " times, should have been found exactly once.\n* Marked object: " +
         marked_object->ToString(true, cached).Utf8() + "\n\n" + tree_str;
}

}  // namespace blink
