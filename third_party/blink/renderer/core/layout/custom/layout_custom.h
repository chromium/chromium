// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_CUSTOM_LAYOUT_CUSTOM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_CUSTOM_LAYOUT_CUSTOM_H_

#include "third_party/blink/renderer/core/layout/custom/css_layout_definition.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

namespace blink {

class LayoutCustomPhaseScope;

// NOTE: In the future there may be a third state "normal", this will mean that
// not everything is blockified, (e.g. root inline boxes, so that line-by-line
// layout can be performed).
enum LayoutCustomState { kUnloaded, kBlock };

// This enum is used to determine if the current layout is under control of web
// developer defined script, or during a fallback layout pass.
// Sizing of children is different between these two phases.
enum LayoutCustomPhase { kCustom, kFallback };

// The LayoutObject for elements which have "display: layout(foo);" specified.
// https://drafts.css-houdini.org/css-layout-api/
//
// This class inherits from LayoutBlockFlow so that when a web developer's
// intrinsicSizes/layout callback fails, it will fallback onto the default
// block-flow layout algorithm.
class LayoutCustom final : public LayoutBlockFlow {
 public:
  explicit LayoutCustom(Element*);

  const char* GetName() const override { return "LayoutCustom"; }
  LayoutCustomState State() const { return state_; }
  LayoutCustomPhase Phase() const { return phase_; }

  bool IsLoaded() const { return State() != LayoutCustomState::kUnloaded; }

  // "ConstraintData" is the additional input data object passed from parent to
  // child layouts. It must be set before a custom layout pass, then
  // immediately cleared.
  SerializedScriptValue* GetConstraintData() const;
  void SetConstraintData(scoped_refptr<SerializedScriptValue> data);
  void ClearConstraintData();

  // "FragmentResultData" is the additional output data object passed from the
  // child to parent.
  SerializedScriptValue* GetFragmentResultData() const;

  bool CreatesNewFormattingContext() const override { return true; }

  void AddChild(LayoutObject* new_child, LayoutObject* before_child) override;
  void RemoveChild(LayoutObject* child) override;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void UpdateBlockLayout(bool relayout_children) override;

 private:
  friend class LayoutCustomPhaseScope;

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutCustom || LayoutBlockFlow::IsOfType(type);
  }

  bool PerformLayout(bool relayout_children, SubtreeLayoutScope*);

  LayoutCustomState state_;
  LayoutCustomPhase phase_;
  Persistent<CSSLayoutDefinition::Instance> instance_;

  scoped_refptr<SerializedScriptValue> constraint_data_;
  scoped_refptr<SerializedScriptValue> fragment_result_data_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutCustom, IsLayoutCustom());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_CUSTOM_LAYOUT_CUSTOM_H_
