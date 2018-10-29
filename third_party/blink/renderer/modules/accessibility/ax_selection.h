// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SELECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SELECTION_H_

#include <base/logging.h>
#include <stdint.h>
#include <ostream>

#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

// If the |AXSelection| is defined by endpoints that are present in the
// accessibility tree but not in the DOM tree, determines whether setting the
// selection will shrink or extend the |AXSelection| to encompass endpoints that
// are in the DOM.
enum class AXSelectionBehavior {
  kShrinkToValidDOMRange,
  kExtendToValidDOMRange
};

class MODULES_EXPORT AXSelection final {
  DISALLOW_NEW();

 public:
  class Builder;

  static const AXSelection FromSelection(
      const SelectionInDOMTree&,
      const AXSelectionBehavior = AXSelectionBehavior::kExtendToValidDOMRange);

  AXSelection(const AXSelection&) = default;
  AXSelection& operator=(const AXSelection&) = default;
  ~AXSelection() = default;

  const AXPosition Base() const { return base_; }
  const AXPosition Extent() const { return extent_; }

  // The selection is invalid if either the anchor or the focus position is
  // invalid, or if the positions are in two separate documents.
  bool IsValid() const;

  operator bool() const { return IsValid(); }

  const SelectionInDOMTree AsSelection(
      const AXSelectionBehavior =
          AXSelectionBehavior::kExtendToValidDOMRange) const;

  // Tries to set the DOM selection to this.
  void Select(
      const AXSelectionBehavior = AXSelectionBehavior::kExtendToValidDOMRange);

 private:
  AXSelection();

  // The |AXPosition| where the selection starts.
  AXPosition base_;

  // The |AXPosition| where the selection ends.
  AXPosition extent_;

#if DCHECK_IS_ON()
  // TODO(accessibility): Use layout tree version in place of DOM and style
  // versions.
  uint64_t dom_tree_version_;
  uint64_t style_version_;
#endif

  friend class Builder;
};

class MODULES_EXPORT AXSelection::Builder final {
  STACK_ALLOCATED();

 public:
  Builder() = default;
  ~Builder() = default;
  Builder& SetBase(const AXPosition&);
  Builder& SetBase(const Position&);
  Builder& SetExtent(const AXPosition&);
  Builder& SetExtent(const Position&);
  Builder& SetSelection(const SelectionInDOMTree&);
  const AXSelection Build();

 private:
  AXSelection selection_;
};

MODULES_EXPORT bool operator==(const AXSelection&, const AXSelection&);
MODULES_EXPORT bool operator!=(const AXSelection&, const AXSelection&);
MODULES_EXPORT std::ostream& operator<<(std::ostream&, const AXSelection&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SELECTION_H_
