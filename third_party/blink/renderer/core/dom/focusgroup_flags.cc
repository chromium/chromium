// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

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
  StringBuilder invalid_tokens;

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
    } else {
      if (!invalid_tokens.IsEmpty())
        invalid_tokens.Append(", ");

      // We don't use |lowercase_token| here since that string value will be
      // logged in the console and we want it to match the input.
      invalid_tokens.Append(WTF::String::FromUTF8(tokens[i].Ascii()));
    }
  }

  if (!invalid_tokens.IsEmpty()) {
    element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            WebString::FromUTF8("Unrecognized focusgroup attribute values: " +
                                invalid_tokens.ToString().Ascii())));
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

  if (has_horizontal && has_vertical) {
    element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kWarning,
            WebString::FromUTF8(
                "'horizontal' and 'vertical' focusgroup attribute values used "
                "together are redundant (this is the default behavior) and can "
                "be omitted.")));
  }

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

    if (!(flags & FocusgroupFlags::kExtend)) {
      element->GetDocument().AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kOther,
              mojom::blink::ConsoleMessageLevel::kWarning,
              WebString::FromUTF8(
                  "Focusgroup attribute value 'extend' present, "
                  "but no parent focusgroup found.")));
    }
  }

  // 5. Set the flag for grid if the value was provided.
  if (has_grid || (flags & FocusgroupFlags::kExtend &&
                   ancestor_flags & FocusgroupFlags::kGrid)) {
    flags |= FocusgroupFlags::kGrid;

    if (ancestor_flags & FocusgroupFlags::kExtend) {
      // We don't support focusgroups that try to extend the grid inner
      // focusgroup.
      element->GetDocument().AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kOther,
              mojom::blink::ConsoleMessageLevel::kError,
              WebString::FromUTF8(
                  "Focusgroup attribute value 'extend' cannot be "
                  "used to extend a parent 'grid' focusgroup.")));
      return FocusgroupFlags::kNone;
    }

    // When in a grid focusgroup, the outer focusgroup should only support one
    // axis and its inner focusgroup should support the other one.
    if (flags & FocusgroupFlags::kExtend) {
      if (ancestor_flags & FocusgroupFlags::kHorizontal) {
        if (flags & FocusgroupFlags::kHorizontal) {
          element->GetDocument().AddConsoleMessage(
              MakeGarbageCollected<ConsoleMessage>(
                  mojom::blink::ConsoleMessageSource::kOther,
                  mojom::blink::ConsoleMessageLevel::kWarning,
                  WebString::FromUTF8(
                      "Focusgroup attribute value 'horizontal' ignored; parent "
                      "'grid' focusgroup already specifies 'horizontal' and "
                      "'vertical' is assumed.")));
        }
        flags &= ~FocusgroupFlags::kHorizontal;
        flags |= FocusgroupFlags::kVertical;
      } else {
        DCHECK(ancestor_flags & FocusgroupFlags::kVertical);
        if (flags & FocusgroupFlags::kVertical) {
          element->GetDocument().AddConsoleMessage(
              MakeGarbageCollected<ConsoleMessage>(
                  mojom::blink::ConsoleMessageSource::kOther,
                  mojom::blink::ConsoleMessageLevel::kWarning,
                  WebString::FromUTF8(
                      "Focusgroup attribute value 'vertical' ignored; parent "
                      "'grid' focusgroup already specifies 'vertical' and "
                      "'horizontal' is assumed.")));
        }
        flags |= FocusgroupFlags::kHorizontal;
        flags &= ~FocusgroupFlags::kVertical;
      }
    } else if (flags & FocusgroupFlags::kHorizontal &&
               flags & FocusgroupFlags::kVertical) {
      // In theory, the author needs to specify an axis on the outer focusgroup,
      // but if they don't we'll revert to a default value of "horizontal".
      flags &= ~FocusgroupFlags::kVertical;
      element->GetDocument().AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kOther,
              mojom::blink::ConsoleMessageLevel::kWarning,
              WebString::FromUTF8(
                  "Focusgroup attribute value 'grid' requires an additional "
                  "'horizontal' or 'vertical' direction value. Using "
                  "'horizontal' as a default value.")));
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
    if ((flags & FocusgroupFlags::kWrapHorizontally) ==
            (ancestor_flags & FocusgroupFlags::kWrapHorizontally) &&
        (flags & FocusgroupFlags::kWrapVertically) ==
            (ancestor_flags & FocusgroupFlags::kWrapVertically)) {
      element->GetDocument().AddConsoleMessage(MakeGarbageCollected<
                                               ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kWarning,
          WebString::FromUTF8(
              "Focusgroup attribute value 'wrap' present but ignored. 'wrap' "
              "is inherited from the extended parent focusgroup.")));
    }
    if (flags & FocusgroupFlags::kHorizontal)
      flags |= (ancestor_flags & FocusgroupFlags::kWrapHorizontally);
    if (flags & FocusgroupFlags::kVertical)
      flags |= (ancestor_flags & FocusgroupFlags::kWrapVertically);
  }

  return flags;
}

}  // namespace blink::focusgroup
