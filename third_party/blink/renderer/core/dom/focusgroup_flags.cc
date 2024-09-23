// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink::focusgroup {

FocusgroupFlags FindNearestFocusgroupAncestorFlags(const Element* element) {
  Element* ancestor = FlatTreeTraversal::ParentElement(*element);
  while (ancestor) {
    FocusgroupFlags ancestor_flags = ancestor->GetFocusgroupFlags();
    // When this is true, we found the focusgroup to extend.
    if (ancestor_flags != FocusgroupFlags::kNone) {
      return ancestor_flags;
    }
    ancestor = FlatTreeTraversal::ParentElement(*ancestor);
  }
  return FocusgroupFlags::kNone;
}

FocusgroupFlags ParseFocusgroup(const Element* element,
                                const AtomicString& input) {
  DCHECK(element);
  ExecutionContext* context = element->GetExecutionContext();
  DCHECK(RuntimeEnabledFeatures::FocusgroupEnabled(context));

  UseCounter::Count(context, WebFeature::kFocusgroup);

  // 1. Parse the input.
  bool has_extend = false;
  bool has_inline = false;
  bool has_block = false;
  bool has_grid = false;
  bool has_wrap = false;
  bool has_row_wrap = false;
  bool has_col_wrap = false;
  bool has_flow = false;
  bool has_row_flow = false;
  bool has_col_flow = false;
  StringBuilder invalid_tokens;

  SpaceSplitString tokens(input);
  for (unsigned i = 0; i < tokens.size(); i++) {
    AtomicString lowercase_token = tokens[i].LowerASCII();
    if (lowercase_token == "extend") {
      has_extend = true;
    } else if (lowercase_token == "inline") {
      has_inline = true;
    } else if (lowercase_token == "block") {
      has_block = true;
    } else if (lowercase_token == "grid") {
      has_grid = true;
    } else if (lowercase_token == "wrap") {
      has_wrap = true;
    } else if (lowercase_token == "row-wrap") {
      has_row_wrap = true;
    } else if (lowercase_token == "col-wrap") {
      has_col_wrap = true;
    } else if (lowercase_token == "flow") {
      has_flow = true;
    } else if (lowercase_token == "row-flow") {
      has_row_flow = true;
    } else if (lowercase_token == "col-flow") {
      has_col_flow = true;
    } else {
      if (!invalid_tokens.empty())
        invalid_tokens.Append(", ");

      // We don't use |lowercase_token| here since that string value will be
      // logged in the console and we want it to match the input.
      invalid_tokens.Append(tokens[i]);
    }
  }

  if (!invalid_tokens.empty()) {
    element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "Unrecognized focusgroup attribute values: " +
                invalid_tokens.ReleaseString()));
  }

  FocusgroupFlags flags = FocusgroupFlags::kNone;

  // 2. Apply the extend logic. A focusgroup can extend another one explicitly
  // when the author specifies "extend" or implicitly when a focusgroup has the
  // "gridcells" role.
  FocusgroupFlags ancestor_flags = FocusgroupFlags::kNone;
  if (has_extend) {
    // Focusgroups should only be allowed to extend when they have a focusgroup
    // ancestor and the focusgroup ancestor isn't a grid focusgroup.
    ancestor_flags = FindNearestFocusgroupAncestorFlags(element);
    if (ancestor_flags != FocusgroupFlags::kNone) {
      flags |= FocusgroupFlags::kExtend;
      if (ancestor_flags & FocusgroupFlags::kGrid) {
        element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kError,
                "Focusgroup attribute value 'extend' present, but grid "
                "focusgroups cannot be extended. Ignoring focusgroup."));
        return FocusgroupFlags::kNone;
      }
    } else {
      element->GetDocument().AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kOther,
              mojom::blink::ConsoleMessageLevel::kError,
              "Focusgroup attribute value 'extend' present, but no parent "
              "focusgroup found. Ignoring 'extend'."));
    }
  }

  // 3. Apply the grid focusgroup logic:
  //     * 'grid' can only be set on an HTML table element.
  //     * The grid-related wrap/flown can only be set on a grid focusgroup.
  if (has_grid) {
    if (has_extend) {
      element->GetDocument().AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kOther,
              mojom::blink::ConsoleMessageLevel::kError,
              "Focusgroup attribute values 'extend' and 'grid' present, but "
              "grid focusgroup cannot extend. Ignoring focusgroup."));
      return FocusgroupFlags::kNone;
    }

    flags |= FocusgroupFlags::kGrid;

    // Set the wrap/flow flags, if specified.
    if (has_wrap) {
      flags |= FocusgroupFlags::kWrapInline | FocusgroupFlags::kWrapBlock;
      if (has_row_wrap) {
        element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kWarning,
                "Focusgroup attribute value 'row-wrap' present, but can be "
                "omitted because focusgroup already wraps in both axes."));
      }
      if (has_col_wrap) {
        element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kWarning,
                "Focusgroup attribute value 'col-wrap' present, but can be "
                "omitted because focusgroup already wraps in both axes."));
      }
    } else {
      if (has_row_wrap)
        flags |= FocusgroupFlags::kWrapInline;
      if (has_col_wrap)
        flags |= FocusgroupFlags::kWrapBlock;

      if (has_row_wrap && has_col_wrap) {
        element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kWarning,
                "Focusgroup attribute values 'row-wrap col-wrap' should be "
                "replaced by 'wrap'."));
      }
    }

    if (has_flow) {
      if (flags & FocusgroupFlags::kWrapInline ||
          flags & FocusgroupFlags::kWrapBlock) {
        element->GetDocument().AddConsoleMessage(MakeGarbageCollected<
                                                 ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "Focusgroup attribute value 'flow' present, but focusgroup already "
            "set to wrap in at least one axis."));
      } else {
        flags |= FocusgroupFlags::kRowFlow | FocusgroupFlags::kColFlow;
        if (has_row_flow) {
          element->GetDocument().AddConsoleMessage(
              MakeGarbageCollected<ConsoleMessage>(
                  mojom::blink::ConsoleMessageSource::kOther,
                  mojom::blink::ConsoleMessageLevel::kWarning,
                  "Focusgroup attribute value 'row-flow' present, but can be "
                  "omitted because focusgroup already flows in both axes."));
        }
        if (has_col_flow) {
          element->GetDocument().AddConsoleMessage(
              MakeGarbageCollected<ConsoleMessage>(
                  mojom::blink::ConsoleMessageSource::kOther,
                  mojom::blink::ConsoleMessageLevel::kWarning,
                  "Focusgroup attribute value 'col-flow' present, but can be "
                  "omitted because focusgroup already flows in both axes."));
        }
      }
    } else {
      if (has_row_flow) {
        if (flags & FocusgroupFlags::kWrapInline) {
          element->GetDocument().AddConsoleMessage(
              MakeGarbageCollected<ConsoleMessage>(
                  mojom::blink::ConsoleMessageSource::kOther,
                  mojom::blink::ConsoleMessageLevel::kError,
                  "Focusgroup attribute value 'row-flow' present, but "
                  "focusgroup already wraps in the row axis."));
        } else {
          flags |= FocusgroupFlags::kRowFlow;
        }
      }
      if (has_col_flow) {
        if (flags & FocusgroupFlags::kWrapBlock) {
          element->GetDocument().AddConsoleMessage(
              MakeGarbageCollected<ConsoleMessage>(
                  mojom::blink::ConsoleMessageSource::kOther,
                  mojom::blink::ConsoleMessageLevel::kError,
                  "Focusgroup attribute value 'col-flow' present, but "
                  "focusgroup already wraps in the column axis."));
        } else {
          flags |= FocusgroupFlags::kColFlow;
        }
      }
      if (flags & FocusgroupFlags::kRowFlow &&
          flags & FocusgroupFlags::kColFlow) {
        element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kWarning,
                "Focusgroup attribute values 'row-flow col-flow' should be "
                "replaced by 'flow'."));
      }
    }

    // These values are reserved for linear focusgroups.
    if (has_inline) {
      element->GetDocument().AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kOther,
              mojom::blink::ConsoleMessageLevel::kError,
              "Focusgroup attribute value 'inline' present, but has no effect "
              "on grid focusgroups."));
    }
    if (has_block) {
      element->GetDocument().AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kOther,
              mojom::blink::ConsoleMessageLevel::kError,
              "Focusgroup attribute value 'block' present, but has no effect "
              "on grid focusgroups."));
    }

    return flags;
  }

  // At this point, we are necessarily in a linear focusgroup. Any grid
  // focusgroup should have returned above.

  if (has_row_wrap) {
    element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "Focusgroup attribute value 'row-wrap' present, but has no effect "
            "on linear focusgroups."));
  }
  if (has_col_wrap) {
    element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "Focusgroup attribute value 'col-wrap' present, but has no effect "
            "on linear focusgroups."));
  }
  if (has_flow) {
    element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "Focusgroup attribute value 'flow' present, but has no effect on "
            "linear focusgroups."));
  }
  if (has_row_flow) {
    element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "Focusgroup attribute value 'row-flow' present, but has no effect "
            "on linear focusgroups."));
  }
  if (has_col_flow) {
    element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "Focusgroup attribute value 'col-flow' present, but has no effect "
            "on linear focusgroups."));
  }

  // 4. Set the axis supported on that focusgroup.
  if (has_inline) {
    flags |= FocusgroupFlags::kInline;
  }
  if (has_block) {
    flags |= FocusgroupFlags::kBlock;
  }

  // When no axis is specified, it means that the focusgroup should handle
  // both.
  if (!has_inline && !has_block) {
    flags |= FocusgroupFlags::kInline | FocusgroupFlags::kBlock;
  }

  if (has_inline && has_block) {
    element->GetDocument().AddConsoleMessage(MakeGarbageCollected<
                                             ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "'inline' and 'block' focusgroup attribute values used together "
        "are redundant (this is the default behavior) and can be omitted."));
  }

  // 6. Determine in what axis a focusgroup should wrap. This needs to be
  // performed once the supported axes are final.
  if (has_wrap) {
    if (flags & FocusgroupFlags::kExtend) {
      bool extends_inline = flags & FocusgroupFlags::kInline &&
                            ancestor_flags & FocusgroupFlags::kInline;
      if (!extends_inline && flags & FocusgroupFlags::kInline) {
        flags |= FocusgroupFlags::kWrapInline;
      }
      bool extends_block = flags & FocusgroupFlags::kBlock &&
                           ancestor_flags & FocusgroupFlags::kBlock;
      if (!extends_block && flags & FocusgroupFlags::kBlock) {
        flags |= FocusgroupFlags::kWrapBlock;
      }

      if (extends_inline && extends_block) {
        element->GetDocument().AddConsoleMessage(MakeGarbageCollected<
                                                 ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Focusgroup attribute value 'wrap' present but ignored. 'wrap' has "
            "no effect when set on a focusgroup that extends another one in "
            "both axes."));
      }
    } else {
      if (flags & FocusgroupFlags::kInline) {
        flags |= FocusgroupFlags::kWrapInline;
      }
      if (flags & FocusgroupFlags::kBlock) {
        flags |= FocusgroupFlags::kWrapBlock;
      }
    }
  }

  // When a focusgroup extends another one, inherit the ancestor's wrap behavior
  // for the descendant's supported axes.
  if (flags & FocusgroupFlags::kExtend) {
    DCHECK(ancestor_flags != FocusgroupFlags::kNone);
    if ((flags & FocusgroupFlags::kWrapInline) ==
            (ancestor_flags & FocusgroupFlags::kWrapInline) &&
        (flags & FocusgroupFlags::kWrapBlock) ==
            (ancestor_flags & FocusgroupFlags::kWrapBlock)) {
      element->GetDocument().AddConsoleMessage(MakeGarbageCollected<
                                               ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "Focusgroup attribute value 'wrap' present but ignored. 'wrap' is "
          "inherited from the extended parent focusgroup."));
    }
    if (flags & FocusgroupFlags::kInline) {
      flags |= (ancestor_flags & FocusgroupFlags::kWrapInline);
    }
    if (flags & FocusgroupFlags::kBlock) {
      flags |= (ancestor_flags & FocusgroupFlags::kWrapBlock);
    }
  }

  return flags;
}

}  // namespace blink::focusgroup
