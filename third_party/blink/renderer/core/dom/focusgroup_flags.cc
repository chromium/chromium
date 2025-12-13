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

namespace {
struct FlagMapping {
  const char* token;
  FocusgroupFlags flag;
};

struct BehaviorMapping {
  const char* token;
  FocusgroupBehavior behavior;
  ax::mojom::blink::Role aria_role;
};

// List of behavior flags and corresponding ARIA role mappings.
// This should be kept in sync with FocusgroupBehavior.
constexpr BehaviorMapping kBehaviorMap[] = {
    {"toolbar", FocusgroupBehavior::kToolbar, ax::mojom::blink::Role::kToolbar},
    {"tablist", FocusgroupBehavior::kTablist, ax::mojom::blink::Role::kTabList},
    {"radiogroup", FocusgroupBehavior::kRadiogroup,
     ax::mojom::blink::Role::kRadioGroup},
    {"listbox", FocusgroupBehavior::kListbox, ax::mojom::blink::Role::kListBox},
    {"menu", FocusgroupBehavior::kMenu, ax::mojom::blink::Role::kMenu},
    {"menubar", FocusgroupBehavior::kMenubar, ax::mojom::blink::Role::kMenuBar},
    {"grid", FocusgroupBehavior::kGrid, ax::mojom::blink::Role::kGrid},
    {"none", FocusgroupBehavior::kOptOut, ax::mojom::blink::Role::kUnknown}};

// Unified mapping of all recognized modifier tokens.
// This should be kept in sync with FocusgroupFlags.
constexpr FlagMapping kModifierMap[] = {
    {"inline", FocusgroupFlags::kInline},
    {"block", FocusgroupFlags::kBlock},
    {"wrap", FocusgroupFlags::kWrapInline | FocusgroupFlags::kWrapBlock},
    {"row-wrap", FocusgroupFlags::kWrapInline},
    {"col-wrap", FocusgroupFlags::kWrapBlock},
    {"flow", FocusgroupFlags::kRowFlow | FocusgroupFlags::kColFlow},
    {"row-flow", FocusgroupFlags::kRowFlow},
    {"col-flow", FocusgroupFlags::kColFlow},
    {"no-memory", FocusgroupFlags::kNoMemory},
};

// Returns true if a flag contains a modifier only meaningful for grid
// focusgroups.
inline bool IsGridOnlyFlag(FocusgroupFlags flag) {
  // The grid behavior and flow flags are grid-only.
  bool any_flow =
      flag & (FocusgroupFlags::kRowFlow | FocusgroupFlags::kColFlow);

  // Wrapping in a single axis is grid-only.
  bool wrap_inline = flag & FocusgroupFlags::kWrapInline;
  bool wrap_block = flag & FocusgroupFlags::kWrapBlock;
  bool exactly_one_wrap = wrap_inline != wrap_block;
  return any_flow || exactly_one_wrap;
}

// Returns a string representation of all valid behavior tokens.
String ValidBehaviorTokenListString(ExecutionContext* context) {
  const bool is_grid_enabled =
      RuntimeEnabledFeatures::FocusgroupGridEnabled(context);

  std::string assembled;
  for (const auto& behavior_mapping : kBehaviorMap) {
    // Filter out grid token when grid is disabled.
    if (!is_grid_enabled &&
        behavior_mapping.behavior == FocusgroupBehavior::kGrid) {
      continue;
    }
    if (!assembled.empty()) {
      assembled.append(", ");
    }
    DCHECK_NE(behavior_mapping.token, String());
    assembled.append(behavior_mapping.token);
  }
  return String(assembled.c_str());
}

String ValidTokenListString(ExecutionContext* context) {
  const bool is_grid_enabled =
      RuntimeEnabledFeatures::FocusgroupGridEnabled(context);

  std::string assembled;
  for (const auto& mapping : kModifierMap) {
    // Filter out grid-only tokens when grid is disabled.
    if (!is_grid_enabled && IsGridOnlyFlag(mapping.flag)) {
      continue;
    }
    if (!assembled.empty()) {
      assembled.append(", ");
    }
    DCHECK_NE(mapping.token, String());
    assembled.append(mapping.token);
  }
  return String(assembled.c_str());
}

// Returns the corresponding flag for a recognized token, or kNone if invalid.
FocusgroupFlags FocusgroupFlagFromString(const AtomicString& token) {
  for (const auto& td : kModifierMap) {
    if (token == td.token) {
      return td.flag;
    }
  }
  return FocusgroupFlags::kNone;
}

FocusgroupBehavior FocusgroupBehaviorFromString(const AtomicString& token) {
  for (const auto& td : kBehaviorMap) {
    if (token == td.token) {
      return td.behavior;
    }
  }
  return FocusgroupBehavior::kNoBehavior;
}

}  // namespace

Element* FindFocusgroupOwner(const Element* element) {
  Element* ancestor = FlatTreeTraversal::ParentElement(*element);
  while (ancestor) {
    if (ancestor->GetFocusgroupData().behavior == FocusgroupBehavior::kOptOut) {
      // If we find a focusgroup opt-out before the actual focusgroup, then the
      // element is opted out.
      return nullptr;
    }
    if (IsActualFocusgroup(ancestor->GetFocusgroupData())) {
      return ancestor;
    }
    ancestor = FlatTreeTraversal::ParentElement(*ancestor);
  }
  return nullptr;
}

FocusgroupData ParseFocusgroup(const Element* element,
                               const AtomicString& input) {
  DCHECK(element);
  ExecutionContext* context = element->GetExecutionContext();
  DCHECK(RuntimeEnabledFeatures::FocusgroupEnabled(context));
  const bool is_grid_enabled =
      RuntimeEnabledFeatures::FocusgroupGridEnabled(context);

  UseCounter::Count(context, WebFeature::kFocusgroup);

  bool has_inline = false;
  bool has_block = false;
  bool has_wrap = false;
  bool has_row_wrap = false;
  bool has_col_wrap = false;
  bool has_flow = false;
  bool has_row_flow = false;
  bool has_col_flow = false;
  bool has_no_memory = false;

  // Helpers to avoid repeated enum boilerplate for console messages.
  auto Warn = [&](const String& msg) {
    element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kWarning, msg));
  };
  auto Error = [&](const String& msg) {
    element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError, msg));
  };

  SpaceSplitString tokens(input);

  // Build a consolidated error message for missing/invalid first token.
  auto FirstTokenErrorMessage = [&]() {
    return String(StrCat({"focusgroup requires a behavior token (",
                          ValidBehaviorTokenListString(context),
                          ") or 'none' as the first value."}));
  };

  // Two step process - first parse all flags, then validate the combination for
  // the given behavior.

  // 1. Parse the input.
  // Handle empty input (no tokens case).
  if (tokens.size() == 0) {
    Error(FirstTokenErrorMessage());
    return {};
  }

  // Validate and consume the first token before iterating the rest.
  AtomicString first_token = tokens[0].LowerASCII();
  // First token is the single allowed behavior.
  FocusgroupData data;
  data.behavior = FocusgroupBehaviorFromString(first_token);

  if (data.behavior == FocusgroupBehavior::kNoBehavior) {
    // Unrecognized first token, emit error and return.
    Error(StrCat({FirstTokenErrorMessage(), " Found: '", first_token, "'."}));
    return {};
  }

  if (data.behavior == FocusgroupBehavior::kGrid && !is_grid_enabled) {
    Error(
        "Focusgroup behavior 'grid' is not supported because the "
        "FocusgroupGrid feature is disabled.");
    return {};
  }

  if (data.behavior == FocusgroupBehavior::kOptOut) {
    if (tokens.size() > 1) {
      Warn(
          "focusgroup attribute value 'none' disables focusgroup behavior; all "
          "other tokens are ignored.");
    }
    return {FocusgroupBehavior::kOptOut, FocusgroupFlags::kNone};
  }

  StringBuilder invalid_tokens;
  // Start at the second token.
  for (unsigned i = 1; i < tokens.size(); i++) {
    AtomicString lowercase_token = tokens[i].LowerASCII();
    // The fist token is always a behavior, subsequent tokens are modifiers.
    FocusgroupFlags flag = FocusgroupFlagFromString(lowercase_token);
    // If this is a grid-only modifier flag and the grid feature is disabled,
    // warn and ignore it (do not classify as invalid for easier to understand
    // warnings).
    if (IsGridOnlyFlag(flag) && !is_grid_enabled) {
      Warn(StrCat({"focusgroup attribute value '", lowercase_token,
                   "' ignored because grid focusgroups are disabled."}));
      continue;
    }

    // Handle unrecognized tokens.
    if (flag == FocusgroupFlags::kNone) {
      if (!invalid_tokens.empty()) {
        invalid_tokens.Append(", ");
      }
      invalid_tokens.Append(tokens[i]);
      continue;
    }

    // Handle other tokens.
    if (flag & FocusgroupFlags::kNoMemory) {
      has_no_memory = true;
    } else if (flag & FocusgroupFlags::kInline) {
      has_inline = true;
    } else if (flag & FocusgroupFlags::kBlock) {
      has_block = true;
    } else if (flag & FocusgroupFlags::kWrapInline &&
               flag & FocusgroupFlags::kWrapBlock) {
      has_wrap = true;
    } else if (flag & FocusgroupFlags::kWrapInline) {
      CHECK(is_grid_enabled);
      has_row_wrap = true;
    } else if (flag & FocusgroupFlags::kWrapBlock) {
      CHECK(is_grid_enabled);
      has_col_wrap = true;
    } else if (flag & FocusgroupFlags::kRowFlow &&
               flag & FocusgroupFlags::kColFlow) {
      CHECK(is_grid_enabled);
      has_flow = true;
    } else if (flag & FocusgroupFlags::kRowFlow) {
      CHECK(is_grid_enabled);
      has_row_flow = true;
    } else if (flag & FocusgroupFlags::kColFlow) {
      CHECK(is_grid_enabled);
      has_col_flow = true;
    }
  }

  if (!invalid_tokens.empty()) {
    StringBuilder builder;
    builder.Append("Unrecognized focusgroup attribute values encountered: ");
    builder.Append(invalid_tokens.ReleaseString());
    builder.Append(". Valid tokens are: ");
    builder.Append(ValidTokenListString(context));
    builder.Append('.');
    Warn(builder.ToString());
  }

  // Set the memory flag before the branch between grid and linear focusgroups.
  if (has_no_memory) {
    data.flags |= FocusgroupFlags::kNoMemory;
  }

  // 2. Go over the set flags and ensure the combination is valid.

  // Grid focusgroup specific validation and flag setting.
  if (data.behavior == FocusgroupBehavior::kGrid) {
    // Set the wrap/flow flags, if specified.
    if (has_wrap) {
      data.flags |= FocusgroupFlags::kWrapInline | FocusgroupFlags::kWrapBlock;
      if (has_row_wrap) {
        Warn(
            "Focusgroup attribute value 'row-wrap' present, but can be "
            "omitted because focusgroup already wraps in both axes.");
      }
      if (has_col_wrap) {
        Warn(
            "Focusgroup attribute value 'col-wrap' present, but can be "
            "omitted because focusgroup already wraps in both axes.");
      }
    } else {
      if (has_row_wrap)
        data.flags |= FocusgroupFlags::kWrapInline;
      if (has_col_wrap)
        data.flags |= FocusgroupFlags::kWrapBlock;

      if (has_row_wrap && has_col_wrap) {
        Warn(
            "Focusgroup attribute values 'row-wrap col-wrap' should be "
            "replaced by 'wrap'.");
      }
    }

    if (has_flow) {
      if (data.flags & FocusgroupFlags::kWrapInline ||
          data.flags & FocusgroupFlags::kWrapBlock) {
        Error(
            "Focusgroup attribute value 'flow' present, but focusgroup already "
            "set to wrap in at least one axis.");
        return {};
      } else {
        data.flags |= FocusgroupFlags::kRowFlow | FocusgroupFlags::kColFlow;
        if (has_row_flow) {
          Warn(
              "Focusgroup attribute value 'row-flow' present, but can be "
              "omitted because focusgroup already flows in both axes.");
        }
        if (has_col_flow) {
          Warn(
              "Focusgroup attribute value 'col-flow' present, but can be "
              "omitted because focusgroup already flows in both axes.");
        }
      }
    } else {
      if (has_row_flow) {
        if (data.flags & FocusgroupFlags::kWrapInline) {
          Error(
              "Focusgroup attribute value 'row-flow' present, but "
              "focusgroup already wraps in the row axis.");
          return {};
        } else {
          data.flags |= FocusgroupFlags::kRowFlow;
        }
      }
      if (has_col_flow) {
        if (data.flags & FocusgroupFlags::kWrapBlock) {
          Error(
              "Focusgroup attribute value 'col-flow' present, but "
              "focusgroup already wraps in the column axis.");
          return {};
        } else {
          data.flags |= FocusgroupFlags::kColFlow;
        }
      }
      if (data.flags & FocusgroupFlags::kRowFlow &&
          data.flags & FocusgroupFlags::kColFlow) {
        Warn(
            "Focusgroup attribute values 'row-flow col-flow' should be "
            "replaced by 'flow'.");
      }
    }

    // These values are reserved for linear focusgroups.
    if (has_inline) {
      Warn(
          "Focusgroup attribute value 'inline' is not valid for grid "
          "focusgroups; use row-wrap/col-wrap or flow modifiers instead.");
    }
    if (has_block) {
      Warn(
          "Focusgroup attribute value 'block' is not valid for grid "
          "focusgroups; use row-wrap/col-wrap or flow modifiers instead.");
    }
    return data;
  }

  // Linear focusgroup specific validation and flag setting.
  if (has_row_wrap) {
    Warn(
        "Focusgroup attribute value 'row-wrap' is only valid for grid "
        "focusgroups; use 'wrap' for linear focusgroups instead.");
  }
  if (has_col_wrap) {
    Warn(
        "Focusgroup attribute value 'col-wrap' is only valid for grid "
        "focusgroups; use 'wrap' for linear focusgroups instead.");
  }
  if (has_flow) {
    Warn(
        "Focusgroup attribute value 'flow' is only valid for grid "
        "focusgroups.");
  }
  if (has_row_flow) {
    Warn(
        "Focusgroup attribute value 'row-flow' is only valid for grid "
        "focusgroups.");
  }
  if (has_col_flow) {
    Warn(
        "Focusgroup attribute value 'col-flow' is only valid for grid "
        "focusgroups.");
  }
  if (has_inline && has_block) {
    Warn(
        "Focusgroup attribute values 'inline' and 'block' used together "
        "are redundant (this is the default behavior for linear focusgroups) "
        "and can be omitted.");
  }

  // When no axis is specified for linear focusgroups, it means that the
  // focusgroup should handle both.
  if (!has_inline && !has_block) {
    data.flags |= FocusgroupFlags::kInline | FocusgroupFlags::kBlock;
  } else {
    if (has_inline) {
      data.flags |= FocusgroupFlags::kInline;
    }
    if (has_block) {
      data.flags |= FocusgroupFlags::kBlock;
    }
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

  return data;
}

String FocusgroupDataToStringForTesting(const FocusgroupData& data) {
  StringBuilder builder;
  // Handle "no behavior" first.
  if (data.behavior == FocusgroupBehavior::kNoBehavior) {
    builder.Append("No behavior");
    return builder.ToString();
  }

  for (const auto& behavior_mapping : kBehaviorMap) {
    if (data.behavior == behavior_mapping.behavior) {
      builder.Append(behavior_mapping.token);
      break;
    }
  }

  builder.Append(':');
  builder.Append(FocusgroupFlagsToStringForTesting(data.flags));
  return builder.ToString();
}

String FocusgroupFlagsToStringForTesting(FocusgroupFlags flags) {
  StringBuilder builder;
  builder.Append('(');
  for (const auto& modifier_mapping : kModifierMap) {
    if (flags & modifier_mapping.flag) {
      // Append '|' if this is not the first flag. (account for the opening
      // paren).
      if (builder.length() > 1) {
        builder.Append('|');
      }
      builder.Append(modifier_mapping.token);
    }
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
  // This function should not be called on non-focusgroups, including opted out
  // elements.
  CHECK(IsActualFocusgroup(data));
  // Return appropriate role based on behavior token mapping.
  for (const auto& behavior_mapping : kBehaviorMap) {
    if (data.behavior == behavior_mapping.behavior) {
      return behavior_mapping.aria_role;
    }
  }
  NOTREACHED() << "Unmapped focusgroup behavior: "
               << static_cast<int>(data.behavior);
}

ax::mojom::blink::Role FocusgroupItemMinimumAriaRole(
    const FocusgroupData& data) {
  switch (data.behavior) {
    case FocusgroupBehavior::kTablist:
      return ax::mojom::blink::Role::kTab;
    case FocusgroupBehavior::kRadiogroup:
      return ax::mojom::blink::Role::kRadioButton;
    case FocusgroupBehavior::kListbox:
      return ax::mojom::blink::Role::kListBoxOption;
    case FocusgroupBehavior::kMenu:
    case FocusgroupBehavior::kMenubar:
      return ax::mojom::blink::Role::kMenuItem;
    case FocusgroupBehavior::kToolbar:
    case FocusgroupBehavior::kGrid:
    case FocusgroupBehavior::kNoBehavior:
    case FocusgroupBehavior::kOptOut:
      return ax::mojom::blink::Role::kUnknown;  // No mapping.
  }
  NOTREACHED()
      << "Unhandled FocusgroupBehavior in FocusgroupItemMinimumAriaRole: "
      << static_cast<int>(data.behavior);
}

}  // namespace blink::focusgroup
