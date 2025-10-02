// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"

#include <ostream>

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"

namespace blink::focusgroup {

FocusgroupData FindNearestFocusgroupAncestorData(const Element* element) {
  Element* ancestor = FlatTreeTraversal::ParentElement(*element);
  while (ancestor) {
    FocusgroupData ancestor_data = ancestor->GetFocusgroupData();
    // When this is true, we found the focusgroup to extend.
    if (IsActualFocusgroup(ancestor_data)) {
      return ancestor_data;
    }
    ancestor = FlatTreeTraversal::ParentElement(*ancestor);
  }
  return {};
}

FocusgroupData ParseFocusgroup(const Element* element,
                               const AtomicString& input) {
  DCHECK(element);
  ExecutionContext* context = element->GetExecutionContext();
  DCHECK(RuntimeEnabledFeatures::FocusgroupEnabled(context));

  UseCounter::Count(context, WebFeature::kFocusgroup);

  // 1. Parse the input.
  bool has_inline = false;
  bool has_block = false;
  bool has_wrap = false;
  bool has_row_wrap = false;
  bool has_col_wrap = false;
  bool has_flow = false;
  bool has_row_flow = false;
  bool has_col_flow = false;
  bool has_no_memory = false;
  StringBuilder invalid_tokens;

  SpaceSplitString tokens(input);

  // Check if first token is a behavior token
  if (tokens.size() == 0) {
    element->GetDocument().AddConsoleMessage(MakeGarbageCollected<
                                             ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kError,
        "Focusgroup attribute requires a behavior token as the first value."));
    return {};
  }

  // Validate that first token is a behavior token
  AtomicString first_token = tokens[0].LowerASCII();
  bool first_token_is_behavior =
      (first_token == "toolbar" || first_token == "tablist" ||
       first_token == "radiogroup" || first_token == "listbox" ||
       first_token == "menu" || first_token == "menubar" ||
       first_token == "grid" || first_token == "none");

  if (!first_token_is_behavior) {
    element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            StrCat({"Focusgroup attribute requires a behavior token (toolbar, "
                    "tablist, radiogroup, listbox, menu, menubar, grid, none) "
                    "as the first value. Found: '",
                    first_token, "'."})));
    return {};
  }

  FocusgroupData data;
  for (unsigned i = 0; i < tokens.size(); i++) {
    AtomicString lowercase_token = tokens[i].LowerASCII();
    if (lowercase_token == "toolbar") {
      if (data.behavior != FocusgroupBehavior::kNoBehavior) {
        element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kError,
                "Focusgroup attribute can only specify one behavior token."));
        return {};
      }
      data.behavior = FocusgroupBehavior::kToolbar;
    } else if (lowercase_token == "tablist") {
      if (data.behavior != FocusgroupBehavior::kNoBehavior) {
        element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kError,
                "Focusgroup attribute can only specify one behavior token."));
        return {};
      }
      data.behavior = FocusgroupBehavior::kTablist;
    } else if (lowercase_token == "radiogroup") {
      if (data.behavior != FocusgroupBehavior::kNoBehavior) {
        element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kError,
                "Focusgroup attribute can only specify one behavior token."));
        return {};
      }
      data.behavior = FocusgroupBehavior::kRadiogroup;
    } else if (lowercase_token == "listbox") {
      if (data.behavior != FocusgroupBehavior::kNoBehavior) {
        element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kError,
                "Focusgroup attribute can only specify one behavior token."));
        return {};
      }
      data.behavior = FocusgroupBehavior::kListbox;
    } else if (lowercase_token == "menu") {
      if (data.behavior != FocusgroupBehavior::kNoBehavior) {
        element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kError,
                "Focusgroup attribute can only specify one behavior token."));
        return {};
      }
      data.behavior = FocusgroupBehavior::kMenu;
    } else if (lowercase_token == "menubar") {
      if (data.behavior != FocusgroupBehavior::kNoBehavior) {
        element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kError,
                "Focusgroup attribute can only specify one behavior token."));
        return {};
      }
      data.behavior = FocusgroupBehavior::kMenubar;
    } else if (lowercase_token == "grid" &&
               RuntimeEnabledFeatures::FocusgroupGridEnabled(
                   element->GetExecutionContext())) {
      if (data.behavior != FocusgroupBehavior::kNoBehavior) {
        element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kError,
                "Focusgroup attribute can only specify one behavior token."));
        return {};
      }
      data.behavior = FocusgroupBehavior::kGrid;
    } else if (lowercase_token == "none") {
      if (data.behavior != FocusgroupBehavior::kNoBehavior) {
        element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kOther,
                mojom::blink::ConsoleMessageLevel::kError,
                "Focusgroup attribute can only specify one behavior token."));
        return {};
      }
      data.behavior = FocusgroupBehavior::kOptOut;
    } else if (lowercase_token == "inline") {
      has_inline = true;
    } else if (lowercase_token == "block") {
      has_block = true;
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
    } else if (lowercase_token == "no-memory") {
      has_no_memory = true;
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
            StrCat({"Unrecognized focusgroup attribute values: ",
                    invalid_tokens.ReleaseString()})));
  }

  // Check if any behavior was specified (required)
  if (data.behavior == FocusgroupBehavior::kNoBehavior) {
    element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "Focusgroup attribute requires a behavior token."));
    return {};
  }

  // Opt-out short-circuits all other semantics. If combined with any other
  // recognized token emit a console message and ignore the others.
  if (data.behavior == FocusgroupBehavior::kOptOut) {
    if (data.flags != FocusgroupFlags::kNone) {
      element->GetDocument().AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kOther,
              mojom::blink::ConsoleMessageLevel::kError,
              "Focusgroup attribute value 'none' cannot be combined with other"
              " focusgroup attribute values; all others ignored."));
    }
    // Return early for opt-out behavior - no additional flags needed
    return data;
  }

  // 2. Apply the grid focusgroup logic:
  //     * 'grid' can only be set on an HTML table element.
  //     * The grid-related wrap/flow can only be set on a grid focusgroup.
  if (data.behavior == FocusgroupBehavior::kGrid) {
    // Set the wrap/flow flags, if specified.
    if (has_wrap) {
      data.flags |= FocusgroupFlags::kWrapInline | FocusgroupFlags::kWrapBlock;
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
        data.flags |= FocusgroupFlags::kWrapInline;
      if (has_col_wrap)
        data.flags |= FocusgroupFlags::kWrapBlock;

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
      if (data.flags & FocusgroupFlags::kWrapInline ||
          data.flags & FocusgroupFlags::kWrapBlock) {
        element->GetDocument().AddConsoleMessage(MakeGarbageCollected<
                                                 ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "Focusgroup attribute value 'flow' present, but focusgroup already "
            "set to wrap in at least one axis."));
      } else {
        data.flags |= FocusgroupFlags::kRowFlow | FocusgroupFlags::kColFlow;
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
        if (data.flags & FocusgroupFlags::kWrapInline) {
          element->GetDocument().AddConsoleMessage(
              MakeGarbageCollected<ConsoleMessage>(
                  mojom::blink::ConsoleMessageSource::kOther,
                  mojom::blink::ConsoleMessageLevel::kError,
                  "Focusgroup attribute value 'row-flow' present, but "
                  "focusgroup already wraps in the row axis."));
        } else {
          data.flags |= FocusgroupFlags::kRowFlow;
        }
      }
      if (has_col_flow) {
        if (data.flags & FocusgroupFlags::kWrapBlock) {
          element->GetDocument().AddConsoleMessage(
              MakeGarbageCollected<ConsoleMessage>(
                  mojom::blink::ConsoleMessageSource::kOther,
                  mojom::blink::ConsoleMessageLevel::kError,
                  "Focusgroup attribute value 'col-flow' present, but "
                  "focusgroup already wraps in the column axis."));
        } else {
          data.flags |= FocusgroupFlags::kColFlow;
        }
      }
      if (data.flags & FocusgroupFlags::kRowFlow &&
          data.flags & FocusgroupFlags::kColFlow) {
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

    if (has_no_memory) {
      data.flags |= FocusgroupFlags::kNoMemory;
    }
    return data;
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

  // 4. Set the axis supported on that linear focusgroup.
  if (has_inline) {
    data.flags |= FocusgroupFlags::kInline;
  }
  if (has_block) {
    data.flags |= FocusgroupFlags::kBlock;
  }

  // When no axis is specified for linear focusgroups, it means that the
  // focusgroup should handle both.
  if (!has_inline && !has_block) {
    data.flags |= FocusgroupFlags::kInline | FocusgroupFlags::kBlock;
  }

  if (has_inline && has_block) {
    element->GetDocument().AddConsoleMessage(MakeGarbageCollected<
                                             ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "'inline' and 'block' focusgroup attribute values used together "
        "are redundant (this is the default behavior) and can be omitted."));
  }

  // 6. Determine in what axis a linear focusgroup should wrap. This needs to be
  // performed once the supported axes are final.
  if (has_wrap) {
    if (data.flags & FocusgroupFlags::kInline) {
      data.flags |= FocusgroupFlags::kWrapInline;
    }
    if (data.flags & FocusgroupFlags::kBlock) {
      data.flags |= FocusgroupFlags::kWrapBlock;
    }
  }

  if (has_no_memory) {
    data.flags |= FocusgroupFlags::kNoMemory;
  }
  return data;
}

String FocusgroupDataToStringForTesting(const FocusgroupData& data) {
  StringBuilder builder;
  builder.Append("FocusgroupData(");
  switch (data.behavior) {
    case FocusgroupBehavior::kToolbar:
      builder.Append("Toolbar");
      break;
    case FocusgroupBehavior::kTablist:
      builder.Append("Tablist");
      break;
    case FocusgroupBehavior::kRadiogroup:
      builder.Append("Radiogroup");
      break;
    case FocusgroupBehavior::kListbox:
      builder.Append("Listbox");
      break;
    case FocusgroupBehavior::kMenu:
      builder.Append("Menu");
      break;
    case FocusgroupBehavior::kMenubar:
      builder.Append("Menubar");
      break;
    case FocusgroupBehavior::kGrid:
      builder.Append("Grid");
      break;
    case FocusgroupBehavior::kOptOut:
      builder.Append("OptOut)");
      return builder.ToString();
    case FocusgroupBehavior::kNoBehavior:
      builder.Append("None)");
      return builder.ToString();
  }

  builder.Append(':');
  builder.Append(FocusgroupFlagsToStringForTesting(data.flags));
  builder.Append(')');
  return builder.ToString();
}

String FocusgroupFlagsToStringForTesting(FocusgroupFlags flags) {
  Vector<const char*> names;
  names.ReserveInitialCapacity(9);
  auto append_flag_name_if_set = [&](FocusgroupFlags flag, const char* name) {
    if (flags & flag) {
      names.push_back(name);
    }
  };

  // Modifier flags only.
  append_flag_name_if_set(FocusgroupFlags::kInline, "Inline");
  append_flag_name_if_set(FocusgroupFlags::kBlock, "Block");
  append_flag_name_if_set(FocusgroupFlags::kWrapInline, "WrapInline");
  append_flag_name_if_set(FocusgroupFlags::kWrapBlock, "WrapBlock");
  append_flag_name_if_set(FocusgroupFlags::kRowFlow, "RowFlow");
  append_flag_name_if_set(FocusgroupFlags::kColFlow, "ColFlow");
  append_flag_name_if_set(FocusgroupFlags::kNoMemory, "NoMemory");
  StringBuilder builder;
  builder.Append("FocusgroupFlags(");
  for (wtf_size_t i = 0; i < names.size(); ++i) {
    if (i) {
      builder.Append('|');
    }
    builder.Append(names[i]);
  }
  builder.Append(')');
  return builder.ToString();
}

bool IsActualFocusgroup(const FocusgroupData& data) {
  // OptOut is a mutually exclusive state used to explicitly disable focusgroup
  // behavior for a subtree. The parser guarantees that if kOptOut is set no
  // other semantic flags are present. This DCHECK defends against accidental
  // combinations.
  DCHECK(!(data.behavior == FocusgroupBehavior::kOptOut) ||
         (data.flags == FocusgroupFlags::kNone));
  return data.behavior != FocusgroupBehavior::kNoBehavior &&
         data.behavior != FocusgroupBehavior::kOptOut;
}

ax::mojom::blink::Role FocusgroupMinimumAriaRole(const FocusgroupData& data) {
  // Return appropriate role based on behavior token.
  if (data.behavior == FocusgroupBehavior::kToolbar) {
    return ax::mojom::blink::Role::kToolbar;
  }
  if (data.behavior == FocusgroupBehavior::kTablist) {
    return ax::mojom::blink::Role::kTabList;
  }
  if (data.behavior == FocusgroupBehavior::kRadiogroup) {
    return ax::mojom::blink::Role::kRadioGroup;
  }
  if (data.behavior == FocusgroupBehavior::kListbox) {
    return ax::mojom::blink::Role::kListBox;
  }
  if (data.behavior == FocusgroupBehavior::kMenu) {
    return ax::mojom::blink::Role::kMenu;
  }
  if (data.behavior == FocusgroupBehavior::kMenubar) {
    return ax::mojom::blink::Role::kMenuBar;
  }
  if (data.behavior == FocusgroupBehavior::kGrid) {
    return ax::mojom::blink::Role::kGrid;
  }

  // Default case should never be reached because this function should only be
  // called with valid focusgroup flags (i.e. not kNone or kOptOut).
  NOTREACHED() << "FocusgroupMinimumAriaRole called with invalid behavior "
               << FocusgroupDataToStringForTesting(data);
}

}  // namespace blink::focusgroup
