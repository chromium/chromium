// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSGROUP_FLAGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSGROUP_FLAGS_H_

#include <iosfwd>
#include <utility>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/accessibility/ax_enums.mojom-blink-forward.h"

namespace blink {

class Element;

namespace focusgroup {

// Mutually exclusive behavior tokens (first token must be one of these).
// These define the semantic role and behavior of the focusgroup.
enum class FocusgroupBehavior : uint8_t {
  kNoBehavior,  // No behavior specified / sentinel.
  kToolbar,
  kTablist,
  kRadiogroup,
  kListbox,
  kMenu,
  kMenubar,
  // Grid behavior gated on FocusgroupGrid runtime feature.
  kGrid,
  // Explicit opt-out (standalone, cannot be combined with any modifiers).
  kOptOut,
};

enum FocusgroupFlags : uint8_t {
  kNone = 0,  // No focusgroup behavior (default / sentinel).

  // Primary navigation axis:
  kInline = 1 << 0,  // Constrain directional focus navigation to inline axis.
  kBlock = 1 << 1,   // Constrain directional focus navigation to block axis.

  // Boundary wrap behaviors (both set for non-grid wrap):
  kWrapInline =
      1 << 2,  // Wrap at inline edge (continue from opposite inline edge).
  kWrapBlock =
      1 << 3,  // Wrap at block edge (continue from opposite block edge).

  // Flow ordering hints (Grid only):
  kRowFlow = 1 << 4,  // Row-major traversal preference.
  kColFlow = 1 << 5,  // Column-major traversal preference.

  // Memory behavior override disables history-based focus restoration:
  kNoMemory = 1 << 6,
};

inline constexpr FocusgroupFlags operator&(FocusgroupFlags a,
                                           FocusgroupFlags b) {
  return static_cast<FocusgroupFlags>(std::to_underlying(a) &
                                      std::to_underlying(b));
}

inline constexpr FocusgroupFlags operator|(FocusgroupFlags a,
                                           FocusgroupFlags b) {
  return static_cast<FocusgroupFlags>(std::to_underlying(a) |
                                      std::to_underlying(b));
}

inline FocusgroupFlags& operator|=(FocusgroupFlags& a, FocusgroupFlags b) {
  return a = a | b;
}

inline FocusgroupFlags& operator&=(FocusgroupFlags& a, FocusgroupFlags b) {
  return a = a & b;
}

inline constexpr FocusgroupFlags operator~(FocusgroupFlags flags) {
  return static_cast<FocusgroupFlags>(~std::to_underlying(flags));
}

struct FocusgroupData {
  FocusgroupBehavior behavior = FocusgroupBehavior::kNoBehavior;
  FocusgroupFlags flags = FocusgroupFlags::kNone;

  bool operator==(const FocusgroupData& other) const = default;
  bool operator!=(const FocusgroupData& other) const = default;
};

// Returns the nearest ancestor Element that is an actual focusgroup owner or
// nullptr if none exists.
CORE_EXPORT Element* FindFocusgroupOwner(const Element* element);

CORE_EXPORT FocusgroupData ParseFocusgroup(const Element* element,
                                           const AtomicString& input);

// Exported helper for tests and logging to obtain a string form.
CORE_EXPORT String FocusgroupDataToStringForTesting(const FocusgroupData& data);
CORE_EXPORT String FocusgroupFlagsToStringForTesting(FocusgroupFlags flags);

// Returns true if the parsed data represents an actual focusgroup (i.e. not
// the empty sentinel and not explicitly opted out via kOptOut).
CORE_EXPORT bool IsActualFocusgroup(const FocusgroupData& data);

// Returns the minimum ARIA role that should be applied to an element with the
// given focusgroup flags.
CORE_EXPORT ax::mojom::blink::Role FocusgroupMinimumAriaRole(
    const FocusgroupData& data);

// Returns the implied ARIA role for an item inside a focusgroup owner whose
// role was itself implied (i.e. generic container with no explicit role
// attribute). Returns kUnknown if no mapping should be implied.
CORE_EXPORT ax::mojom::blink::Role FocusgroupItemMinimumAriaRole(
    const FocusgroupData& data);
}  // namespace focusgroup

// The "::blink" prefix is to avoid false-positive of audit_non_blink_usages.py.
using FocusgroupFlags = ::blink::focusgroup::FocusgroupFlags;
using FocusgroupBehavior = ::blink::focusgroup::FocusgroupBehavior;
using FocusgroupData = ::blink::focusgroup::FocusgroupData;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSGROUP_FLAGS_H_
