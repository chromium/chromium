// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FOCUSGROUP_CONTROLLER_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FOCUSGROUP_CONTROLLER_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Element;
class GridFocusgroupStructureInfo;
class KeyboardEvent;

enum class FocusgroupDirection {
  kNone,
  kBackwardInline,
  kBackwardBlock,
  kForwardInline,
  kForwardBlock,
};

enum class FocusgroupType {
  kGrid,
  kLinear,
};

class CORE_EXPORT FocusgroupControllerUtils {
  STATIC_ONLY(FocusgroupControllerUtils);

 public:
  static FocusgroupDirection FocusgroupDirectionForEvent(KeyboardEvent* event);
  static bool IsDirectionBackward(FocusgroupDirection direction);
  static bool IsDirectionForward(FocusgroupDirection direction);
  static bool IsDirectionInline(FocusgroupDirection direction);
  static bool IsDirectionBlock(FocusgroupDirection direction);
  static bool IsAxisSupported(FocusgroupFlags flags,
                              FocusgroupDirection direction);
  static bool WrapsInDirection(FocusgroupFlags flags,
                               FocusgroupDirection direction);
  static Element* FindNearestFocusgroupAncestor(const Element* element,
                                                FocusgroupType type);
  static Element* NextElementInDirection(const Element* current,
                                         FocusgroupDirection direction,
                                         bool skip_subtree);
  static Element* NextElement(const Element* current, bool skip_subtree);
  static Element* PreviousElement(const Element* current,
                                  bool skip_subtree = false);

  // Returns the next focusgroup item candidate within the |owner| subtree
  // relative to |current_item| in the traversal order implied by |direction|.
  // For forward directions this returns the first subsequent eligible item.
  // For backward directions this returns the last eligible item that appears
  // before |current|. Returns nullptr if none is found. Nested focusgroup
  // subtrees are skipped entirely.
  static Element* NextFocusgroupItemInDirection(const Element* owner,
                                                const Element* current_item,
                                                FocusgroupDirection direction);

  // Returns the element to focus when wrapping within |owner| in the given
  // |direction| starting from |current|. Skips nested focusgroup subtrees and
  // never returns |current| itself. Returns nullptr if no alternative item
  // exists.
  static Element* WrappedFocusgroupCandidate(const Element* owner,
                                             const Element* current,
                                             FocusgroupDirection direction);

  // Returns the first/last focusgroup item within |owner|'s scope, or nullptr
  // if no eligible item exists. |owner| must itself be a focusgroup owner.
  static Element* FirstFocusgroupItemWithin(const Element* owner);
  static Element* LastFocusgroupItemWithin(const Element* owner);

  static Element* AdjustElementOutOfUnrelatedFocusgroup(
      Element* element,
      Element* stop_ancestor,
      FocusgroupDirection direction);

  static bool IsFocusgroupItemWithOwner(const Element* element,
                                        const Element* focusgroup_owner);
  static bool IsGridFocusgroupItem(const Element* element);

  // Returns true if the element is opted out or within an opted-out focusgroup
  // subtree.
  static bool IsElementInOptedOutSubtree(const Element* element);

  static GridFocusgroupStructureInfo*
  CreateGridFocusgroupStructureInfoForGridRoot(Element* root);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FOCUSGROUP_CONTROLLER_UTILS_H_
