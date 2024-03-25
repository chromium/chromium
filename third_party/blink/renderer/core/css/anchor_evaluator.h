// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ANCHOR_EVALUATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ANCHOR_EVALUATOR_H_

#include <optional>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_anchor_query_enums.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class AnchorQuery;
class AnchorScope;
class ComputedStyleBuilder;
class ScopedCSSName;

class CORE_EXPORT AnchorEvaluator {
  DISALLOW_NEW();

 public:
  AnchorEvaluator() = default;

  // The evaluation of anchor() and anchor-size() functions is affected
  // by the context they are used in. For example, it is not allowed to
  // do anchor() queries "cross-axis" (e.g. left:anchor(--a top)),
  // and anchor-size() queries are only valid in sizing properties.
  // Queries that violate these rules instead resolve to their fallback
  // values (or 0px if no fallback value exists).
  //
  // The default mode of AnchorEvaluator (kNone) is to return nullopt (i.e.
  // fallback) for any query. This represents a context where no anchor query
  // is valid, e.g. a property unrelated to insets or sizing.
  //
  // The values kLeft, kRight, kTop and kBottom represent the corresponding
  // inset properties, and allow anchor() queries [1] (with restrictions),
  // but not anchor-size() queries.
  //
  // The value kSize represents supported sizing properties [2], and allows
  // anchor-size(), but not anchor().
  //
  // The current mode can be set by placing an AnchorScope object on the
  // stack.
  //
  // [1] https://drafts.csswg.org/css-anchor-position-1/#anchor-valid
  // [2] https://drafts.csswg.org/css-anchor-position-1/#anchor-size-valid
  enum class Mode {
    kNone,

    // anchor()
    kLeft,
    kRight,
    kTop,
    kBottom,

    // anchor-size()
    kSize
  };

  // Evaluates an anchor() or anchor-size() query.
  // Returns |nullopt| if the query is invalid (e.g., no targets or wrong
  // axis.), in which case the fallback should be used.
  virtual std::optional<LayoutUnit> Evaluate(const AnchorQuery&) = 0;

  virtual void Trace(Visitor*) const;

 protected:
  explicit AnchorEvaluator(const ScopedCSSName* position_anchor_name)
      : position_anchor_name_(position_anchor_name) {}

  const ScopedCSSName* GetPositionAnchorName() const {
    return position_anchor_name_;
  }
  Mode GetMode() const { return mode_; }

 private:
  friend class AnchorScope;

  // The computed position-anchor in use for the current try option.
  Member<const ScopedCSSName> position_anchor_name_;
  // The computed position-anchor in use for the current try option.
  Mode mode_ = Mode::kNone;
};

// Temporarily changes Mode, default anchor (position-anchor), and apply the
// current inset-area to modify the containing block position / size. When going
// out of scope the AnchorEvaluator is reset back to its previous state with
// caches that need to be invalidated cleared.
//
// If the anchor_evaluator is nullptr the AnchorScope should have no effect.
//
// See AnchorEvaluator::Mode for more information.
class CORE_EXPORT AnchorScope {
  STACK_ALLOCATED();

 public:
  using Mode = AnchorEvaluator::Mode;

  // Temporarily change Mode, applied inset-area and position-anchor.
  AnchorScope(Mode, const ComputedStyleBuilder&, AnchorEvaluator*);
  AnchorScope(Mode,
              const ScopedCSSName* position_anchor_name,
              AnchorEvaluator*);
  // Temporarily change Mode only. Applied inset-area and position-anchor stay
  // the same.
  AnchorScope(Mode, AnchorEvaluator*);

  ~AnchorScope() {
    if (anchor_evaluator_) {
      anchor_evaluator_->mode_ = original_mode_;
      anchor_evaluator_->position_anchor_name_ = original_position_anchor_name_;
    }
  }

 private:
  AnchorEvaluator* anchor_evaluator_ = nullptr;
  const ScopedCSSName* original_position_anchor_name_ = nullptr;
  Mode original_mode_ = Mode::kNone;
  // TODO(329584105): Add inset-area
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ANCHOR_EVALUATOR_H_
