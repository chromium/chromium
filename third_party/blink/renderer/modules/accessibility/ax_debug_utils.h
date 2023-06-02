// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_DEBUG_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_DEBUG_UTILS_H_

#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace blink {

class AXObject;

// Friendly output of a subtree, useful for debugging.
std::string TreeToStringHelper(const AXObject* obj, bool verbose = true);
std::string TreeToStringWithMarkedObjectHelper(const AXObject* obj,
                                               const AXObject* marked_object,
                                               bool verbose = true);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_DEBUG_UTILS_H_
