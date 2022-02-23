// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FOCUSGROUP_CONTROLLER_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FOCUSGROUP_CONTROLLER_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class KeyboardEvent;

enum class FocusgroupDirection {
  kNone,
  kBackwardHorizontal,
  kBackwardVertical,
  kForwardHorizontal,
  kForwardVertical,
};

class CORE_EXPORT FocusgroupControllerUtils {
  STATIC_ONLY(FocusgroupControllerUtils);

 public:
  static FocusgroupDirection FocusgroupDirectionForEvent(KeyboardEvent* event);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FOCUSGROUP_CONTROLLER_UTILS_H_