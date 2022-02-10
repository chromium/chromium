// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink::focusgroup {

FocusgroupFlags ParseFocusgroup(const Element* element,
                                const AtomicString& input) {
  DCHECK(RuntimeEnabledFeatures::FocusgroupEnabled());
  DCHECK(element);

  // 1. Parse the input.
  bool has_extend = false;
  bool has_horizontal = false;
  bool has_vertical = false;
  bool has_wrap = false;
  bool has_grid = false;
  bool has_none = false;

  SpaceSplitString tokens(input);
  for (unsigned i = 0; i < tokens.size(); i++) {
    AtomicString lowercase_token = tokens[i].LowerASCII();
    if (lowercase_token == "extend") {
      has_extend = true;
    } else if (lowercase_token == "horizontal") {
      has_horizontal = true;
    } else if (lowercase_token == "vertical") {
      has_vertical = true;
    } else if (lowercase_token == "wrap") {
      has_wrap = true;
    } else if (lowercase_token == "grid") {
      has_grid = true;
    } else if (lowercase_token == "none") {
      has_none = true;
    }
  }

  // 2. When the focusgroup is explicitly set to none, we should ignore any
  // other flag and only return that value.
  if (has_none)
    return FocusgroupFlags::kExplicitlyNone;

  FocusgroupFlags flags = FocusgroupFlags::kNone;

  // 3. Set the axis supported on that focusgroup.
  if (has_horizontal)
    flags |= FocusgroupFlags::kHorizontal;
  if (has_vertical)
    flags |= FocusgroupFlags::kVertical;

  // When no axis is specified, it means that the focusgroup should handle both.
  if (!has_horizontal && !has_vertical)
    flags |= FocusgroupFlags::kHorizontal | FocusgroupFlags::kVertical;

  // 4. Apply the extend logic.
  FocusgroupFlags ancestor_flags = FocusgroupFlags::kNone;
  if (has_extend) {
    // Focusgroups should only be allowed to extend when they have a focusgroup
    // ancestor.
    Element* ancestor = Traversal<Element>::FirstAncestor(*element);
    while (ancestor) {
      ancestor_flags = ancestor->GetFocusgroupFlags();
      // When this is true, we found the focusgroup to extend.
      if (focusgroup::IsFocusgroup(ancestor_flags)) {
        flags |= FocusgroupFlags::kExtend;
        break;
      }

      // When this is true, it means that the current focusgroup can't extend,
      // because its closest ancestor is one that forbids itself and its subtree
      // from being part of an ancestor's focusgroup.
      if (ancestor_flags & FocusgroupFlags::kExplicitlyNone)
        break;

      ancestor = Traversal<Element>::FirstAncestor(*ancestor);
    }
  }

  // 5. Set the flag for grid if the value was provided.
  if (has_grid || (flags & FocusgroupFlags::kExtend &&
                   ancestor_flags & FocusgroupFlags::kGrid)) {
    flags |= FocusgroupFlags::kGrid;

    if (ancestor_flags & FocusgroupFlags::kExtend) {
      // We don't support focusgroups that try to extend the grid inner
      // focusgroup.
      return FocusgroupFlags::kNone;
    }

    // When in a grid focusgroup, the outer focusgroup should only support one
    // axis and its inner focusgroup should support the other one.
    if (flags & FocusgroupFlags::kExtend) {
      if (ancestor_flags & FocusgroupFlags::kHorizontal) {
        flags &= ~FocusgroupFlags::kHorizontal;
        flags |= FocusgroupFlags::kVertical;
      } else {
        DCHECK(ancestor_flags & FocusgroupFlags::kVertical);
        flags |= FocusgroupFlags::kHorizontal;
        flags &= ~FocusgroupFlags::kVertical;
      }
    } else if (flags & FocusgroupFlags::kHorizontal &&
               flags & FocusgroupFlags::kVertical) {
      // In theory, the author needs to specify an axis on the outer focusgroup,
      // but if they don't we'll revert to a default value of "horizontal".
      flags &= ~FocusgroupFlags::kVertical;
    }
  }

  // 6. Determine in what axis a focusgroup should wrap. This needs to be
  // performed once the supported axes are final.
  if (has_wrap) {
    if (flags & FocusgroupFlags::kHorizontal)
      flags |= FocusgroupFlags::kWrapHorizontally;
    if (flags & FocusgroupFlags::kVertical)
      flags |= FocusgroupFlags::kWrapVertically;
  }

  // When a focusgroup extends another one, inherit the ancestor's wrap behavior
  // for the descendant's supported axes.
  if (flags & FocusgroupFlags::kExtend) {
    DCHECK(focusgroup::IsFocusgroup(ancestor_flags));
    if (flags & FocusgroupFlags::kHorizontal)
      flags |= (ancestor_flags & FocusgroupFlags::kWrapHorizontally);
    if (flags & FocusgroupFlags::kVertical)
      flags |= (ancestor_flags & FocusgroupFlags::kWrapVertically);
  }

  return flags;
}

}  // namespace blink::focusgroup
