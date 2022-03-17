// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FOCUSGROUP_CONTROLLER_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FOCUSGROUP_CONTROLLER_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Element;
class KeyboardEvent;

enum FocusgroupFlags : uint8_t;

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
  static bool IsDirectionBackward(FocusgroupDirection direction);
  static bool IsDirectionForward(FocusgroupDirection direction);
  static bool IsDirectionHorizontal(FocusgroupDirection direction);
  static bool IsDirectionVertical(FocusgroupDirection direction);
  static bool IsAxisSupported(FocusgroupFlags flags,
                              FocusgroupDirection direction);
  static bool WrapsInDirection(FocusgroupFlags flags,
                               FocusgroupDirection direction);
  static bool FocusgroupExtendsInAxis(FocusgroupFlags extending_focusgroup,
                                      FocusgroupFlags focusgroup,
                                      FocusgroupDirection direction);

  static Element* FindNearestFocusgroupAncestor(const Element* element);
  static Element* NextElement(const Element* current, bool skip_subtree);
  static Element* PreviousElement(const Element* current);
  static Element* LastElementWithin(const Element* current);
  static bool IsFocusgroupItem(const Element* element);
  static Element* AdjustElementOutOfUnrelatedFocusgroup(
      Element* element,
      Element* stop_ancestor,
      FocusgroupDirection direction);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FOCUSGROUP_CONTROLLER_UTILS_H_